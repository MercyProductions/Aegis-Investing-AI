import express, { Request, Response } from 'express';
import cors from 'cors';
import crypto from 'node:crypto';
import { createServer } from 'node:http';
import { z } from 'zod';
import { WebSocket, WebSocketServer } from 'ws';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import type { AppState, DataFreshness, OrderSide, SavedReport } from './types.js';
import { Store } from './store.js';
import { MarketEngine } from './market.js';
import { dataQualityForAssets, fetchProviderCandles, providerDiagnostics } from './marketData.js';
import { Trader } from './trader.js';
import { LiveTrader } from './liveTrader.js';
import { ArbitrageEngine } from './arbitrage.js';
import { evaluateExecutionGate } from './safetyGate.js';
import { runBacktest } from './strategy.js';
import { apiResponse, attachCoreContracts, buildDashboardPayload, buildSymbolResearch, compactWebState } from './coreContract.js';
import { readNativeCoreStatus } from './coreBridge.js';
import {
  buildEndOfDayReview,
  buildMorningBriefing,
  buildSymbolTimeline,
  composeResearchReport,
  type ResearchWorkflowInputs
} from './researchWorkflows.js';
import { buildPlatformHealth, createValidatedBackup } from './platformMaturity.js';

const host = process.env.AURALITH_TRADER_HOST || process.env.AEGIS_TRADER_HOST || '127.0.0.1';
const port = Number(process.env.AURALITH_TRADER_PORT || process.env.AEGIS_TRADER_PORT || 4280);
const startedAtMs = Date.now();
const store = new Store();
const market = new MarketEngine(store);
const trader = new Trader(store);
const liveTrader = new LiveTrader(store);
const arbitrage = new ArbitrageEngine();
const SESSION_COOKIE = 'auralith_session';
const CSRF_COOKIE = 'auralith_csrf';
const authRateLimit = new Map<string, number[]>();

market.seed();
market.refreshRealQuotes(true).catch((error: any) => {
  store.audit('warning', 'market.startup_refresh_failed', `Startup market refresh failed: ${String(error)}`);
});
setInterval(() => {
  market.refreshRealQuotes().catch((error: any) => {
    store.audit('warning', 'market.interval_refresh_failed', `Interval market refresh failed: ${String(error)}`);
  });
}, 30000).unref();

const app = express();
app.use(cors({ origin: ['http://127.0.0.1:5176', 'http://localhost:5176'], credentials: true }));
app.use(express.json({ limit: '1mb' }));

app.get('/api/auth/csrf', (req: Request, res: Response) => {
  res.json({ ok: true, csrfToken: ensureCsrfToken(req, res) });
});

app.use((req: Request, res: Response, next) => {
  if (!req.path.startsWith('/api/') || ['GET', 'HEAD', 'OPTIONS'].includes(req.method)) {
    next();
    return;
  }
  const cookieToken = parseCookies(req)[CSRF_COOKIE];
  const headerToken = req.get('x-csrf-token') || '';
  if (!cookieToken || !headerToken || cookieToken !== headerToken) {
    store.audit('warning', 'security.csrf_blocked', `Blocked mutation without valid CSRF token: ${req.method} ${req.path}`);
    res.status(403).json({ ok: false, error: 'Security token expired. Refresh Auralith and try again.' });
    return;
  }
  next();
});

app.post('/api/auth/register', (req: Request, res: Response) => {
  const schema = z.object({
    email: z.string().trim().email().max(254),
    password: z.string().min(8).max(128),
    workspaceName: z.string().trim().min(1).max(80),
    rememberDevice: z.boolean().optional()
  });
  try {
    const body = schema.parse(req.body);
    enforceAuthRateLimit(req, body.email);
    const result = store.createAuthUser(
      body.email,
      body.password,
      body.workspaceName,
      req.get('user-agent') || 'Auralith Web',
      body.rememberDevice === true
    );
    setSessionCookie(res, result.session.token, result.session.expiresAt);
    ensureCsrfToken(req, res);
    res.json({ ok: true, ...publicAuthResult(result) });
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Could not create account.';
    res.status(message.includes('already exists') ? 409 : 400).json({ ok: false, error: message });
  }
});

app.post('/api/auth/login', (req: Request, res: Response) => {
  const schema = z.object({
    email: z.string().trim().email().max(254),
    password: z.string().min(1).max(128),
    rememberDevice: z.boolean().optional()
  });
  try {
    const body = schema.parse(req.body);
    enforceAuthRateLimit(req, body.email);
    const result = store.loginAuthUser(
      body.email,
      body.password,
      req.get('user-agent') || 'Auralith Web',
      body.rememberDevice === true
    );
    setSessionCookie(res, result.session.token, result.session.expiresAt);
    ensureCsrfToken(req, res);
    res.json({ ok: true, ...publicAuthResult(result) });
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Could not sign in.';
    res.status(message.includes('incorrect') ? 401 : 400).json({ ok: false, error: message });
  }
});

app.get('/api/auth/session', (req: Request, res: Response) => {
  const token = authTokenFromRequest(req);
  if (!token) {
    res.status(401).json({ ok: false, error: 'No active Auralith session.' });
    return;
  }
  const user = store.authUserForToken(token);
  if (!user) {
    res.status(401).json({ ok: false, error: 'Auralith session expired or was revoked.' });
    return;
  }
  res.json({ ok: true, user });
});

app.get('/api/auth/sessions', (req: Request, res: Response) => {
  const token = authTokenFromRequest(req);
  if (!token) {
    res.status(401).json({ ok: false, error: 'No active Auralith session.' });
    return;
  }
  const sessions = store.authSessionsForToken(token);
  if (!sessions.length) {
    res.status(401).json({ ok: false, error: 'Auralith session expired or was revoked.' });
    return;
  }
  res.json({ ok: true, sessions });
});

app.post('/api/auth/logout', (req: Request, res: Response) => {
  const token = authTokenFromRequest(req);
  if (token) store.revokeAuthSession(token);
  clearSessionCookie(res);
  res.json({ ok: true });
});

app.post('/api/auth/logout-all', (req: Request, res: Response) => {
  const token = authTokenFromRequest(req);
  if (token) store.revokeAllAuthSessionsForToken(token);
  clearSessionCookie(res);
  res.json({ ok: true });
});

app.get('/api/health', (_req: Request, res: Response) => {
  res.json({
    ok: true,
    app: 'Auralith Markets',
    mode: store.settings().mode,
    liveTradingLocked: store.settings().liveTradingLocked,
    manualConfirmationRequired: store.settings().manualConfirmationRequired,
    generatedAt: new Date().toISOString()
  });
});

const DEFAULT_SIGNAL_LIMIT = 1200;

async function coreState(forceCore = false, signalUniverseLimit: number | null = DEFAULT_SIGNAL_LIMIT, includeSymbols: string[] = []) {
  const state = await trader.state({ signalUniverseLimit: signalUniverseLimit ?? undefined, includeSymbols });
  const nativeCore = await readNativeCoreStatus(forceCore);
  return withPersistentCoreContracts(attachCoreContracts(state, nativeCore));
}

async function webState(req: Request, forceCore = false) {
  const state = await coreState(forceCore, req.query.full === '1' ? null : DEFAULT_SIGNAL_LIMIT);
  return req.query.full === '1' ? state : compactWebState(state);
}

app.get('/api/state', async (req: Request, res: Response) => {
  await market.refreshRealQuotes();
  res.json(await webState(req));
});

app.post('/api/tick', async (req: Request, res: Response) => {
  await market.refreshRealQuotes(true);
  await trader.strategyCycle();
  const state = await webState(req, true);
  broadcastLive('scanner-refresh', { provider: liveProviderPayload(), generatedAt: state.generatedAt });
  res.json(state);
});

app.get('/api/core', async (_req: Request, res: Response) => {
  res.json(apiResponse((await coreState()).core));
});

app.get('/api/core/dashboard', async (_req: Request, res: Response) => {
  res.json(apiResponse(buildDashboardPayload(await coreState())));
});

app.get('/api/core/quotes', async (_req: Request, res: Response) => {
  const state = await coreState();
  res.json(apiResponse({ providerHealth: state.core.providerHealth, quotes: state.market }));
});

app.get('/api/core/watchlists', async (_req: Request, res: Response) => {
  res.json(apiResponse((await coreState()).core.watchlists));
});

app.get('/api/core/scanner', async (_req: Request, res: Response) => {
  res.json(apiResponse((await coreState()).core.scanner));
});

app.get('/api/core/portfolio', async (_req: Request, res: Response) => {
  const state = await coreState();
  res.json(apiResponse({ snapshot: state.core.portfolio, positions: state.positions, orders: state.core.paperOrders }));
});

app.get('/api/core/alerts', async (_req: Request, res: Response) => {
  res.json(apiResponse((await coreState()).core.alerts));
});

app.get('/api/core/reports', async (_req: Request, res: Response) => {
  res.json(apiResponse((await coreState()).core.reports));
});

app.get('/api/core/journal', async (_req: Request, res: Response) => {
  res.json(apiResponse((await coreState()).core.journal));
});

app.get('/api/core/symbol/:symbol', async (req: Request, res: Response) => {
  const symbol = String(req.params.symbol).toUpperCase();
  const state = await coreState(false, DEFAULT_SIGNAL_LIMIT, [symbol]);
  res.json(apiResponse(buildSymbolResearch(symbol, state, state.core.alerts, state.core.journal)));
});

app.get('/api/core/provider-health', async (_req: Request, res: Response) => {
  res.json(apiResponse((await coreState(true)).core.providerHealth));
});

app.get('/api/providers/diagnostics', (_req: Request, res: Response) => {
  const marketState = store.market();
  res.json({
    ok: true,
    diagnostics: providerDiagnostics(),
    dataQuality: dataQualityForAssets(marketState),
    generatedAt: new Date().toISOString()
  });
});

app.get('/api/data-quality', (_req: Request, res: Response) => {
  const marketState = store.market();
  res.json({
    ok: true,
    dataQuality: dataQualityForAssets(marketState),
    quotes: marketState.map((asset) => ({
      symbol: asset.symbol,
      assetClass: asset.assetClass,
      provider: asset.metadata?.provider ?? asset.source,
      fetchedAt: asset.metadata?.fetchedAt ?? asset.updatedAt,
      freshness: asset.metadata?.freshness ?? 'fallback',
      confidence: asset.metadata?.confidence ?? 50,
      delayed: asset.metadata?.delayed ?? true,
      fallback: asset.metadata?.fallback ?? asset.source === 'simulated',
      error: asset.metadata?.error
    })),
    generatedAt: new Date().toISOString()
  });
});

app.get('/api/platform/health', async (_req: Request, res: Response) => {
  const state = await coreState(true);
  res.json({
    ok: true,
    health: buildPlatformHealth(store, state, providerDiagnostics(), startedAtMs)
  });
});

app.post('/api/platform/backup/validate', (_req: Request, res: Response) => {
  const result = createValidatedBackup(store);
  res.status(result.ok ? 200 : 500).json({ ok: result.ok, backup: result });
});

app.get('/api/core/paper-orders', async (_req: Request, res: Response) => {
  res.json(apiResponse((await coreState()).core.paperOrders));
});

app.get('/api/core/ai-summaries', async (_req: Request, res: Response) => {
  res.json(apiResponse((await coreState()).core.aiSummaries));
});

app.get('/api/workflows/morning', async (_req: Request, res: Response) => {
  const state = await coreState();
  res.json({ ok: true, workflow: buildMorningBriefing(state, workflowInputs()) });
});

app.get('/api/workflows/eod', async (_req: Request, res: Response) => {
  const state = await coreState();
  res.json({ ok: true, workflow: buildEndOfDayReview(state, workflowInputs()) });
});

app.get('/api/research/timeline/:symbol', async (req: Request, res: Response) => {
  const symbol = String(req.params.symbol).toUpperCase();
  const state = await coreState(false, DEFAULT_SIGNAL_LIMIT, [symbol]);
  res.json({ ok: true, symbol, timeline: buildSymbolTimeline(symbol, state, workflowInputs()) });
});

app.get('/api/backtest', async (_req: Request, res: Response) => {
  const symbols = store.market().map(m => m.symbol);
  const endTime = Date.now();
  const startTime = endTime - (30 * 24 * 60 * 60 * 1000); // 30 days
  const historicalData: any[] = [];
  for (const symbol of symbols) {
    const data = store.getHistoricalPrices(symbol, startTime, endTime);
    historicalData.push(...data.map(d => ({ symbol, ...d })));
  }
  if (historicalData.length === 0) {
    // Fallback to current market
    const current = store.market();
    historicalData.push(...current.map(m => ({ symbol: m.symbol, timestamp: Date.now(), price: m.price, volume: m.volume })));
  }
  const result = await runBacktest(historicalData);
  res.json(result);
});

const alertTypeSchema = z.enum(['price_above', 'price_below', 'percent_move', 'provider_stale', 'risk_threshold', 'paper_order_update']);
const alertSeveritySchema = z.enum(['info', 'warning', 'error']);

app.get('/api/alerts', (_req: Request, res: Response) => {
  res.json({ ok: true, alerts: store.alerts(), history: store.alertHistory() });
});

app.post('/api/alerts', (req: Request, res: Response) => {
  const schema = z.object({
    type: alertTypeSchema,
    symbol: z.string().trim().max(16).optional(),
    threshold: z.number().finite().optional(),
    percentMove: z.number().finite().optional(),
    severity: alertSeveritySchema.optional(),
    enabled: z.boolean().optional(),
    note: z.string().max(500).optional()
  });
  try {
    const alert = store.createAlert(schema.parse(req.body));
    broadcastLive('alerts', { alert, action: 'created' });
    res.json({ ok: true, alert });
  } catch (error) {
    res.status(400).json({ ok: false, error: error instanceof Error ? error.message : 'Could not create alert.' });
  }
});

app.patch('/api/alerts/:id', (req: Request, res: Response) => {
  const schema = z.object({
    type: alertTypeSchema.optional(),
    symbol: z.string().trim().max(16).optional(),
    threshold: z.number().finite().nullable().optional(),
    percentMove: z.number().finite().nullable().optional(),
    severity: alertSeveritySchema.optional(),
    enabled: z.boolean().optional(),
    note: z.string().max(500).optional()
  });
  try {
    const alert = store.updateAlert(String(req.params.id), schema.parse(req.body) as any);
    broadcastLive('alerts', { alert, action: 'updated' });
    res.json({ ok: true, alert });
  } catch (error) {
    res.status(404).json({ ok: false, error: error instanceof Error ? error.message : 'Alert not found.' });
  }
});

app.delete('/api/alerts/:id', (req: Request, res: Response) => {
  const deleted = store.deleteAlert(String(req.params.id));
  if (deleted) broadcastLive('alerts', { id: String(req.params.id), action: 'deleted' });
  res.status(deleted ? 200 : 404).json(deleted ? { ok: true } : { ok: false, error: 'Alert not found.' });
});

app.post('/api/alerts/:id/acknowledge', (req: Request, res: Response) => {
  const alert = store.acknowledgeAlert(String(req.params.id));
  broadcastLive('alerts', { alert, action: 'acknowledged' });
  res.json({ ok: true, alert });
});

app.post('/api/alerts/:id/snooze', (req: Request, res: Response) => {
  const schema = z.object({ minutes: z.number().int().min(1).max(10080).default(60) });
  const { minutes } = schema.parse(req.body ?? {});
  const alert = store.snoozeAlert(String(req.params.id), minutes);
  broadcastLive('alerts', { alert, action: 'snoozed' });
  res.json({ ok: true, alert });
});

app.get('/api/journal', (req: Request, res: Response) => {
  res.json({ ok: true, entries: store.journalEntries(200, { symbol: String(req.query.symbol ?? '') || undefined, tag: String(req.query.tag ?? '') || undefined }) });
});

app.post('/api/journal', (req: Request, res: Response) => {
  const schema = z.object({
    symbol: z.string().trim().max(16).optional(),
    entryType: z.enum(['note', 'trade_review', 'lesson', 'mistake', 'research']).optional(),
    tags: z.array(z.string().max(40)).max(12).optional(),
    note: z.string().trim().min(1).max(5000),
    mistake: z.string().max(1000).optional(),
    lesson: z.string().max(1000).optional(),
    grade: z.string().max(40).optional(),
    realizedPnl: z.number().finite().optional()
  });
  try {
    res.json({ ok: true, entry: store.createJournalEntry(schema.parse(req.body)) });
  } catch (error) {
    res.status(400).json({ ok: false, error: error instanceof Error ? error.message : 'Could not create journal entry.' });
  }
});

app.patch('/api/journal/:id', (req: Request, res: Response) => {
  const schema = z.object({
    symbol: z.string().trim().max(16).optional(),
    entryType: z.enum(['note', 'trade_review', 'lesson', 'mistake', 'research']).optional(),
    tags: z.array(z.string().max(40)).max(12).optional(),
    note: z.string().trim().min(1).max(5000).optional(),
    mistake: z.string().max(1000).optional(),
    lesson: z.string().max(1000).optional(),
    grade: z.string().max(40).optional(),
    realizedPnl: z.number().finite().optional()
  });
  try {
    res.json({ ok: true, entry: store.updateJournalEntry(String(req.params.id), schema.parse(req.body)) });
  } catch (error) {
    res.status(404).json({ ok: false, error: error instanceof Error ? error.message : 'Journal entry not found.' });
  }
});

app.delete('/api/journal/:id', (req: Request, res: Response) => {
  const deleted = store.deleteJournalEntry(String(req.params.id));
  res.status(deleted ? 200 : 404).json(deleted ? { ok: true } : { ok: false, error: 'Journal entry not found.' });
});

app.get('/api/reports', (_req: Request, res: Response) => {
  res.json({ ok: true, reports: store.savedReports() });
});

app.post('/api/reports', async (req: Request, res: Response) => {
  const schema = z.object({
    kind: z.enum(['morning', 'eod', 'weekly', 'risk', 'strategy', 'symbol']),
    symbol: z.string().trim().max(16).optional()
  });
  const body = schema.parse(req.body);
  const state = await coreState(false, DEFAULT_SIGNAL_LIMIT, body.symbol ? [body.symbol.toUpperCase()] : []);
  const report = store.saveReport(composeReport(body.kind, state, body.symbol?.toUpperCase()));
  res.json({ ok: true, report });
});

app.get('/api/reports/:id', (req: Request, res: Response) => {
  const report = store.reportById(String(req.params.id));
  res.status(report ? 200 : 404).json(report ? { ok: true, report } : { ok: false, error: 'Report not found.' });
});

app.get('/api/reports/:id/export.html', (req: Request, res: Response) => {
  const report = store.reportById(String(req.params.id));
  if (!report) {
    res.status(404).send('Report not found.');
    return;
  }
  res.type('html').send(report.html);
});

app.get('/api/candles/:symbol', async (req: Request, res: Response) => {
  const symbol = String(req.params.symbol).toUpperCase();
  const timeframe = String(req.query.timeframe ?? '1d');
  const candles = await ensureCandles(symbol, timeframe);
  res.json({ ok: true, candles });
});

app.get('/api/watchlists', (_req: Request, res: Response) => {
  res.json({ ok: true, watchlists: store.watchlists() });
});

app.post('/api/watchlists', (req: Request, res: Response) => {
  const schema = z.object({
    name: z.string().trim().min(1).max(80),
    role: z.string().max(180).optional(),
    tags: z.array(z.string().max(40)).max(12).optional(),
    symbols: z.array(z.string().max(16)).max(250).optional()
  });
  res.json({ ok: true, watchlist: store.createWatchlist(schema.parse(req.body)) });
});

app.patch('/api/watchlists/:id', (req: Request, res: Response) => {
  const schema = z.object({
    name: z.string().trim().min(1).max(80).optional(),
    role: z.string().max(180).optional(),
    tags: z.array(z.string().max(40)).max(12).optional(),
    symbols: z.array(z.string().max(16)).max(250).optional()
  });
  try {
    res.json({ ok: true, watchlist: store.updateWatchlist(String(req.params.id), schema.parse(req.body)) });
  } catch (error) {
    res.status(404).json({ ok: false, error: error instanceof Error ? error.message : 'Watchlist not found.' });
  }
});

app.delete('/api/watchlists/:id', (req: Request, res: Response) => {
  const deleted = store.deleteWatchlist(String(req.params.id));
  res.status(deleted ? 200 : 404).json(deleted ? { ok: true } : { ok: false, error: 'Watchlist not found.' });
});

app.get('/api/strategies', (_req: Request, res: Response) => {
  res.json({ ok: true, strategies: store.savedStrategies(), performance: store.getStrategyPerformance() });
});

app.post('/api/strategies', (req: Request, res: Response) => {
  const schema = z.object({
    id: z.string().optional(),
    name: z.string().trim().min(1).max(100),
    description: z.string().max(500).optional(),
    preset: z.string().max(80).optional(),
    enabled: z.boolean().optional(),
    rules: z.array(z.string().max(160)).max(40).optional()
  });
  res.json({ ok: true, strategy: store.saveStrategy(schema.parse(req.body)) });
});

app.delete('/api/strategies/:id', (req: Request, res: Response) => {
  const deleted = store.deleteStrategy(String(req.params.id));
  res.status(deleted ? 200 : 404).json(deleted ? { ok: true } : { ok: false, error: 'Strategy not found.' });
});

app.get('/api/layouts', (_req: Request, res: Response) => {
  res.json({ ok: true, layouts: store.workspaceLayouts() });
});

app.post('/api/layouts', (req: Request, res: Response) => {
  const schema = z.object({
    id: z.string().optional(),
    name: z.string().trim().min(1).max(100),
    mode: z.enum(['daily', 'market-open', 'research', 'portfolio-review', 'strategy-testing', 'mobile-compact']),
    config: z.unknown().optional()
  });
  res.json({ ok: true, layout: store.saveWorkspaceLayout(schema.parse(req.body)) });
});

app.get('/api/notifications', (_req: Request, res: Response) => {
  res.json({ ok: true, notifications: store.notifications() });
});

app.post('/api/notifications/:id/read', (req: Request, res: Response) => {
  store.markNotificationRead(String(req.params.id));
  res.json({ ok: true });
});

app.post('/api/portfolio/cash', async (req: Request, res: Response) => {
  const schema = z.object({ cash: z.number().finite().min(0).max(100000000) });
  const { cash } = schema.parse(req.body);
  store.updatePaperCash(cash);
  res.json(await webState(req));
});

app.get('/api/arbitrage', (_req: Request, res: Response) => {
  res.json(arbitrage.getOpportunities());
});

app.post('/api/arbitrage/execute', (req: Request, res: Response) => {
  const { opportunity, manualConfirmed } = req.body;
  const gate = evaluateExecutionGate(store.settings(), { intent: 'arbitrage-simulation', manualConfirmed });
  if (!gate.allowed) {
    store.audit('warning', gate.auditType, `Arbitrage simulation blocked: ${gate.reason}`, { opportunity });
    res.status(403).json({ error: gate.reason });
    return;
  }
  if (opportunity) {
    const success = arbitrage.executeArbitrage(opportunity);
    store.audit('info', 'arbitrage.paper_simulated', 'Arbitrage simulation executed after manual confirmation.', { opportunity });
    res.json({ success });
  } else {
    res.status(400).json({ error: 'No opportunity provided' });
  }
});

app.post('/api/live-order', async (req: Request, res: Response) => {
  const { symbol, side, qty } = req.body as { symbol: string; side: OrderSide; qty: number };
  try {
    const result = await liveTrader.placeLiveOrder(symbol, side, qty);
    res.json(result);
  } catch (error) {
    res.status(500).json({ error: (error as Error).message });
  }
});

app.post('/api/market/quote', async (req: Request, res: Response) => {
  const schema = z.object({
    symbol: z.string().trim().min(1).max(16).regex(/^[A-Za-z0-9./-]+$/)
  });
  const { symbol } = schema.parse(req.body);
  await market.addStockSymbol(symbol);
  const state = await webState(req, true);
  broadcastLive('quotes', { symbol: symbol.toUpperCase(), provider: liveProviderPayload() });
  res.json(state);
});

app.post('/api/settings', async (req: Request, res: Response) => {
  const schema = z.object({
    autopilot: z.boolean().optional(),
    maxPositionPct: z.number().min(0.01).max(0.5).optional(),
    maxTradeNotional: z.number().min(10).max(100000).optional(),
    maxOpenPositions: z.number().int().min(1).max(30).optional(),
    dailyLossLimitPct: z.number().min(0.001).max(0.25).optional(),
    minConfidence: z.number().min(0.1).max(0.99).optional()
  });
  store.updateSettings(schema.parse(req.body));
  res.json(await webState(req));
});

app.post('/api/orders', async (req: Request, res: Response) => {
  const schema = z.object({
    symbol: z.string().min(1),
    side: z.enum(['buy', 'sell']),
    qty: z.number().positive(),
    signal: z.string().optional(),
    manualConfirmed: z.boolean().optional()
  });
  const order = schema.parse(req.body);
  await trader.placeOrder(order.symbol, order.side, order.qty, order.signal, order.manualConfirmed === true);
  const state = await webState(req);
  broadcastLive('paper-orders', { orders: state.orders.slice(0, 12), account: state.account });
  res.json(state);
});

app.post('/api/reset', async (req: Request, res: Response) => {
  store.resetPaperAccount();
  res.json(await webState(req));
});

const __dirname = dirname(fileURLToPath(import.meta.url));
const clientDir = join(__dirname, '../client');
app.use(express.static(clientDir));
app.use((_req: Request, res: Response) => {
  res.sendFile(join(clientDir, 'index.html'));
});

const httpServer = createServer(app);
const liveSocket = new WebSocketServer({ server: httpServer, path: '/api/live' });

liveSocket.on('connection', (socket) => {
  socket.send(JSON.stringify({
    type: 'hello',
    generatedAt: new Date().toISOString(),
    payload: {
      message: 'Auralith live provider channel connected.',
      provider: liveProviderPayload()
    }
  }));
});

setInterval(() => {
  broadcastLive('provider-status', liveProviderPayload());
}, 15000).unref();

httpServer.listen(port, host, () => {
  console.log(`Auralith Markets running at http://${host}:${port}`);
});

function broadcastLive(type: string, payload: unknown) {
  const message = JSON.stringify({ type, payload, generatedAt: new Date().toISOString() });
  for (const client of liveSocket.clients) {
    if (client.readyState === WebSocket.OPEN) client.send(message);
  }
}

function liveProviderPayload() {
  const marketState = store.market();
  const dataQuality = dataQualityForAssets(marketState);
  const latest = Math.max(0, ...marketState.map((asset) => Date.parse(asset.metadata?.fetchedAt ?? asset.updatedAt)).filter(Number.isFinite));
  const providers = [...new Set(marketState.map((asset) => asset.metadata?.provider ?? asset.source).filter(Boolean))].slice(0, 3);
  const freshness: DataFreshness =
    dataQuality.counts.failed ? 'failed' :
      dataQuality.counts.fallback ? 'fallback' :
        dataQuality.counts.stale ? 'stale' :
          dataQuality.counts.delayed ? 'delayed' :
            dataQuality.counts.live ? 'live' : 'failed';
  return {
    diagnostics: providerDiagnostics(),
    dataQuality,
    providerHealth: {
      source: providers.length ? providers.join(' / ') : 'No providers',
      freshness,
      quoteCount: marketState.length,
      confidence: dataQuality.confidence,
      lastRefresh: latest ? new Date(latest).toISOString() : new Date().toISOString(),
      live: dataQuality.counts.live > 0,
      fallback: dataQuality.counts.fallback > 0,
      delayed: dataQuality.counts.delayed > 0 || dataQuality.counts.stale > 0 || dataQuality.counts.fallback > 0,
      status: dataQuality.warnings.length ? dataQuality.warnings.join(' ') : 'Provider metadata is within configured research bounds.'
    },
    quoteCount: marketState.length
  };
}

function authTokenFromRequest(req: Request) {
  const cookieToken = parseCookies(req)[SESSION_COOKIE];
  if (cookieToken) return cookieToken;
  const header = req.get('authorization') || '';
  const match = header.match(/^Bearer\s+(.+)$/i);
  return match?.[1]?.trim() || '';
}

function parseCookies(req: Request) {
  const header = req.get('cookie') || '';
  return Object.fromEntries(
    header
      .split(';')
      .map((part) => part.trim())
      .filter(Boolean)
      .map((part) => {
        const index = part.indexOf('=');
        const key = index >= 0 ? part.slice(0, index) : part;
        const value = index >= 0 ? part.slice(index + 1) : '';
        return [key, decodeURIComponent(value)];
      })
  ) as Record<string, string>;
}

function ensureCsrfToken(req: Request, res: Response) {
  const existing = parseCookies(req)[CSRF_COOKIE];
  const token = existing || crypto.randomBytes(24).toString('hex');
  if (!existing) {
    setCookie(res, CSRF_COOKIE, token, {
      httpOnly: false,
      sameSite: 'Lax',
      path: '/',
      maxAge: 60 * 60 * 24 * 7
    });
  }
  return token;
}

function setSessionCookie(res: Response, token: string, expiresAt: string) {
  setCookie(res, SESSION_COOKIE, token, {
    httpOnly: true,
    sameSite: 'Lax',
    path: '/',
    expires: new Date(expiresAt)
  });
}

function clearSessionCookie(res: Response) {
  setCookie(res, SESSION_COOKIE, '', {
    httpOnly: true,
    sameSite: 'Lax',
    path: '/',
    expires: new Date(0)
  });
}

function setCookie(
  res: Response,
  name: string,
  value: string,
  options: { httpOnly: boolean; sameSite: 'Lax' | 'Strict' | 'None'; path: string; maxAge?: number; expires?: Date }
) {
  const parts = [`${name}=${encodeURIComponent(value)}`, `Path=${options.path}`, `SameSite=${options.sameSite}`];
  if (options.httpOnly) parts.push('HttpOnly');
  if (process.env.AURALITH_SECURE_COOKIES === '1' || process.env.NODE_ENV === 'production') parts.push('Secure');
  if (options.maxAge !== undefined) parts.push(`Max-Age=${Math.max(0, Math.floor(options.maxAge))}`);
  if (options.expires) parts.push(`Expires=${options.expires.toUTCString()}`);
  res.append('Set-Cookie', parts.join('; '));
}

function publicAuthResult(result: ReturnType<Store['loginAuthUser']>) {
  const { token: _token, ...session } = result.session;
  return { user: result.user, session };
}

function enforceAuthRateLimit(req: Request, email: string) {
  const key = `${clientIp(req)}:${email.toLowerCase()}`;
  const nowMs = Date.now();
  const recent = (authRateLimit.get(key) ?? []).filter((timestamp) => nowMs - timestamp < 10 * 60 * 1000);
  recent.push(nowMs);
  authRateLimit.set(key, recent);
  if (recent.length > 8) {
    store.audit('warning', 'auth.rate_limited', `Auth rate limit triggered for ${email}.`, { ip: clientIp(req) });
    throw new Error('Too many sign-in attempts. Wait a few minutes and try again.');
  }
}

function clientIp(req: Request) {
  return (req.get('x-forwarded-for') || req.socket.remoteAddress || 'local').split(',')[0].trim();
}

function withPersistentCoreContracts(state: AppState): AppState {
  const persistedAlerts = store.alerts().map((alert) => ({
    id: alert.id,
    severity: alert.severity,
    source: 'Auralith web alert workflow',
    timestamp: alert.lastTriggeredAt ?? alert.updatedAt,
    relatedSymbol: alert.symbol,
    title: titleFromSnake(alert.type),
    detail: alertDetailFromRecord(alert),
    acknowledged: Boolean(alert.acknowledgedAt),
    snoozedUntil: alert.snoozedUntil
  }));
  const persistedJournal = store.journalEntries().map((entry) => ({
    id: entry.id,
    symbol: entry.symbol,
    createdAt: entry.createdAt,
    tag: entry.tags[0] ?? entry.entryType,
    grade: entry.grade,
    note: entry.note,
    source: 'Auralith web journal'
  }));
  const persistedReports = store.savedReports().map((report) => ({
    id: report.id,
    title: report.title,
    kind: report.kind,
    status: report.status,
    generatedAt: report.createdAt,
    source: report.source,
    summary: report.summary
  }));
  const persistedWatchlists = store.watchlists().map((watchlist) => ({
    id: watchlist.id,
    name: watchlist.name,
    role: watchlist.role,
    symbols: watchlist.symbols,
    movers: watchlist.symbols.map((symbol) => {
      const asset = state.market.find((item) => item.symbol === symbol);
      return {
        symbol,
        changePct: asset?.changePct ?? 0,
        price: asset?.price ?? 0,
        freshness: asset?.metadata?.freshness ?? 'fallback' as DataFreshness
      };
    })
  }));
  return {
    ...state,
    core: {
      ...state.core,
      watchlists: [...persistedWatchlists, ...state.core.watchlists].slice(0, 24),
      alerts: [...persistedAlerts, ...state.core.alerts].slice(0, 120),
      journal: [...persistedJournal, ...state.core.journal].slice(0, 120),
      reports: [...persistedReports, ...state.core.reports].slice(0, 60)
    }
  };
}

function workflowInputs(): ResearchWorkflowInputs {
  return {
    journal: store.journalEntries(200),
    alertHistory: store.alertHistory(200),
    reports: store.savedReports(100),
    strategies: store.savedStrategies()
  };
}

function composeReport(kind: SavedReport['kind'], state: AppState, symbol?: string) {
  return composeResearchReport(kind, state, workflowInputs(), symbol);
}

async function ensureCandles(symbol: string, timeframe: string) {
  const cached = store.candles(symbol, timeframe, 180);
  if (cached.length >= 30) return cached;
  const asset = store.market().find((item) => item.symbol === symbol);

  try {
    const bundle = await fetchProviderCandles(symbol, timeframe, asset?.assetClass);
    for (const candle of bundle.data) store.upsertCandle(candle);
    store.audit('info', 'candles.synced', `${symbol} ${timeframe} candles synced from ${bundle.quality.provider}.`, {
      symbol,
      timeframe,
      provider: bundle.quality.provider,
      freshness: bundle.quality.freshness,
      fallback: bundle.quality.fallback,
      synthetic: bundle.quality.synthetic
    });
    return store.candles(symbol, timeframe, 180);
  } catch (error) {
    store.audit('warning', 'candles.provider_failed', `${symbol} ${timeframe} provider candles failed; synthetic history was created.`, {
      symbol,
      timeframe,
      error: error instanceof Error ? error.message : String(error)
    });
  }

  const nowMs = Date.now();
  const price = asset?.price ?? 100;
  const volatility = Math.max(0.008, asset?.volatility ?? 0.025);
  for (let index = 179; index >= 0; index -= 1) {
    const timestamp = nowMs - index * 24 * 60 * 60 * 1000;
    const drift = (asset?.momentum ?? 0) * (1 - index / 180);
    const wave = Math.sin(index * 0.23 + price * 0.01) * volatility * price * 1.8;
    const close = Math.max(0.01, price * (1 - drift * 0.08) + wave);
    const open = Math.max(0.01, close * (1 + Math.sin(index * 0.41) * volatility * 0.55));
    const high = Math.max(open, close) * (1 + volatility * 0.7);
    const low = Math.min(open, close) * (1 - volatility * 0.7);
    store.upsertCandle({
      symbol,
      assetType: asset?.assetClass ?? 'stock',
      timeframe,
      timestamp,
      open,
      high,
      low,
      close,
      volume: asset?.volume ?? 1000000,
      provider: asset?.metadata?.provider ?? asset?.source ?? 'Auralith synthetic history',
      fetchedAt: new Date(timestamp).toISOString(),
      freshness: asset?.metadata?.freshness ?? 'fallback',
      fallback: asset?.metadata?.fallback ?? true,
      synthetic: true
    });
  }
  return store.candles(symbol, timeframe, 180);
}

function titleFromSnake(value: string) {
  return value.split('_').map((part) => part.charAt(0).toUpperCase() + part.slice(1)).join(' ');
}

function alertDetailFromRecord(alert: ReturnType<Store['alerts']>[number]) {
  const prefix = alert.symbol ? `${alert.symbol}: ` : '';
  if (alert.type === 'price_above') return `${prefix}price above ${alert.threshold ?? 'threshold'}. ${alert.note}`;
  if (alert.type === 'price_below') return `${prefix}price below ${alert.threshold ?? 'threshold'}. ${alert.note}`;
  if (alert.type === 'percent_move') return `${prefix}move exceeds ${alert.percentMove ?? 'configured'}%. ${alert.note}`;
  if (alert.type === 'provider_stale') return `${prefix}provider freshness is stale. ${alert.note}`;
  if (alert.type === 'risk_threshold') return `${prefix}risk threshold requires review. ${alert.note}`;
  return `${prefix}paper order lifecycle update. ${alert.note}`;
}
