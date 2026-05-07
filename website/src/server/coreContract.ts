import type {
  AppState,
  AuditEvent,
  CoreAiSummary,
  CoreAlertItem,
  CoreApiResponse,
  CoreContracts,
  CoreJournalEntry,
  CorePortfolioSnapshot,
  CoreProviderStatus,
  CoreReportSummary,
  CoreSymbolResearch,
  CoreWatchlistGroup,
  DataFreshness,
  MarketAsset,
  OrderRecord,
  Position,
  RiskSettings,
  Signal,
  StrategyPerformance
} from './types.js';
import type { NativeCoreStatus } from './coreBridge.js';
import { now } from './store.js';

const CONTRACT_VERSION = 'auralith-core-web-v1' as const;

export function apiResponse<T>(data: T): CoreApiResponse<T> {
  return {
    ok: true,
    contractVersion: CONTRACT_VERSION,
    source: 'AuralithCore',
    generatedAt: now(),
    data
  };
}

export function attachCoreContracts(
  state: Omit<AppState, 'core'>,
  nativeCore: NativeCoreStatus | null
): AppState {
  return {
    ...state,
    core: buildCoreContracts(state, nativeCore)
  };
}

export function buildCoreContracts(
  state: Omit<AppState, 'core'>,
  nativeCore: NativeCoreStatus | null
): CoreContracts {
  const providerHealth = buildProviderStatus(state.market, nativeCore);
  const alerts = buildAlerts(state.audit);
  const journal = buildJournal(state.audit, state.orders);
  const reports = buildReports(state, providerHealth);
  const aiSummaries = buildAiSummaries(state, providerHealth);
  const symbolResearch = buildTrackedSymbolResearch(state, alerts, journal);
  return {
    contractVersion: CONTRACT_VERSION,
    generatedAt: now(),
    platform: {
      desktopPrimary: true,
      webRole: 'Polished universal-access cockpit',
      sharedCore: nativeCore ? 'AuralithCore.exe connected' : 'AuralithCore-compatible fallback contract',
      nextExtraction: 'Scanner engine'
    },
    safety: {
      paperOnly: state.settings.mode === 'paper',
      manualConfirmationRequired: state.settings.manualConfirmationRequired,
      liveTradingLocked: state.settings.liveTradingLocked,
      advisoryNotice: 'Auralith is a research-first paper execution platform. It is not financial advice.'
    },
    providerHealth,
    watchlists: buildWatchlists(state.market, state.signals),
    scanner: state.signals,
    portfolio: buildPortfolioSnapshot(state.account, state.positions, state.settings),
    alerts,
    reports,
    journal,
    symbolResearch,
    paperOrders: state.orders,
    aiSummaries
  };
}

export function buildDashboardPayload(state: AppState) {
  const marketRibbonSymbols = new Set(['SPY', 'QQQ', 'DIA', 'VIX']);
  const marketRibbon = state.market
    .filter((asset) => marketRibbonSymbols.has(asset.symbol))
    .concat(state.market.slice(0, 12))
    .filter(uniqueAsset)
    .slice(0, 12);
  return {
    generatedAt: state.generatedAt,
    account: state.account,
    safety: state.core.safety,
    providerHealth: state.core.providerHealth,
    portfolio: state.core.portfolio,
    marketRibbon,
    watchlists: state.core.watchlists,
    scannerHighlights: state.core.scanner.slice(0, 12),
    aiSummaries: state.core.aiSummaries,
    alerts: state.core.alerts.slice(0, 12),
    reports: state.core.reports,
    paperOrders: state.core.paperOrders.slice(0, 12),
    trust: buildTrustSnapshot(state)
  };
}

export function compactWebState(state: AppState): AppState {
  const importantSymbols = new Set<string>();
  state.positions.forEach((position) => importantSymbols.add(position.symbol));
  state.orders.slice(0, 40).forEach((order) => importantSymbols.add(order.symbol));
  state.core.watchlists.flatMap((group) => group.symbols).forEach((symbol) => importantSymbols.add(symbol));
  state.signals.slice(0, 160).forEach((signal) => importantSymbols.add(signal.symbol));
  ['SPY', 'QQQ', 'DIA', 'VIX'].forEach((symbol) => importantSymbols.add(symbol));

  const market = state.market
    .filter((asset, index) => index < 160 || importantSymbols.has(asset.symbol))
    .sort((a, b) => {
      const aImportant = importantSymbols.has(a.symbol) ? 1 : 0;
      const bImportant = importantSymbols.has(b.symbol) ? 1 : 0;
      if (aImportant !== bImportant) return bImportant - aImportant;
      return Math.abs(b.changePct) - Math.abs(a.changePct);
    })
    .slice(0, 320);
  const marketSymbols = new Set(market.map((asset) => asset.symbol));
  const keepSignal = (signal: Signal, index: number) => index < 160 || marketSymbols.has(signal.symbol);

  return {
    ...state,
    market,
    signals: state.signals.filter(keepSignal).slice(0, 220),
    orders: state.orders.slice(0, 80),
    audit: state.audit.slice(0, 100),
    strategyPerformance: state.strategyPerformance.slice(0, 60),
    core: {
      ...state.core,
      scanner: state.core.scanner.filter(keepSignal).slice(0, 220),
      alerts: state.core.alerts.slice(0, 100),
      reports: state.core.reports.slice(0, 40),
      journal: state.core.journal.slice(0, 100),
      symbolResearch: state.core.symbolResearch.slice(0, 96),
      paperOrders: state.core.paperOrders.slice(0, 80),
      aiSummaries: state.core.aiSummaries.slice(0, 20)
    }
  };
}

export function buildSymbolResearch(
  symbol: string,
  state: Omit<AppState, 'core'>,
  alerts = buildAlerts(state.audit),
  journal = buildJournal(state.audit, state.orders)
): CoreSymbolResearch {
  const normalized = symbol.toUpperCase();
  const quote = state.market.find((asset) => asset.symbol === normalized);
  const signal = state.signals.find((item) => item.symbol === normalized);
  const provider = quote?.metadata?.provider ?? quote?.source ?? 'unavailable';
  const freshness = quote?.metadata?.freshness ?? 'failed';
  const aiExplanation = buildSymbolAiSummary(normalized, quote, signal, freshness);
  return {
    symbol: normalized,
    quote,
    signal,
    quoteFreshness: freshness,
    provider,
    momentumScore: signal?.momentumScore ?? (quote ? Math.round(Math.max(-1, Math.min(1, quote.momentum)) * 50 + 50) : 0),
    fundamentals: [
      { label: 'Market Cap', value: quote?.marketCap ? compact(quote.marketCap) : 'Unavailable', source: provider },
      { label: 'Volume', value: quote ? compact(quote.volume) : 'Unavailable', source: provider },
      { label: 'Asset Class', value: quote?.assetClass ?? 'Unknown', source: 'AuralithCore contract' }
    ],
    earnings: [
      { label: 'Calendar', value: 'Provider integration pending', source: 'AuralithCore roadmap' },
      { label: 'Event Risk', value: signal?.risk ?? 'Review data quality', source: 'Scanner engine' }
    ],
    news: [
      { title: 'News and sentiment feed will attach through the Core provider layer.', source: 'AuralithCore roadmap', freshness }
    ],
    secLinks: quote?.assetClass === 'stock'
      ? [{ title: `${normalized} SEC company search`, url: `https://www.sec.gov/edgar/search/#/q=${encodeURIComponent(normalized)}`, source: 'SEC EDGAR' }]
      : [],
    alerts: alerts.filter((alert) => alert.relatedSymbol === normalized),
    journalNotes: journal.filter((entry) => entry.symbol === normalized),
    paperTradePlan: signal ? {
      setupType: signal.setupType,
      currentPrice: signal.currentPrice,
      entry: signal.entry,
      stop: signal.stop,
      target1: signal.target1,
      target2: signal.target2,
      trailingStop: signal.trailingStop,
      expectedSellZone: signal.expectedSellZone,
      invalidation: signal.invalidation,
      exitPlan: signal.exitPlan
    } : undefined,
    aiExplanation
  };
}

function buildProviderStatus(market: MarketAsset[], nativeCore: NativeCoreStatus | null): CoreProviderStatus {
  const latest = latestTimestamp(market);
  const fallback = market.filter((asset) => asset.metadata?.fallback || asset.source === 'simulated').length;
  const averageConfidence = average(market.map((asset) => asset.metadata?.confidence ?? (asset.source === 'simulated' ? 55 : 70)));
  const nativeFreshness = normalizeFreshness(nativeCore?.providerFreshness);
  return {
    source: nativeCore?.providerSource || providerLabel(market),
    freshness: nativeFreshness ?? freshnessFromAge(latest, fallback > 0),
    quoteCount: nativeCore?.quoteCount || market.length,
    confidence: nativeCore?.providerConfidence || Math.round(averageConfidence),
    lastRefresh: latest ? new Date(latest).toISOString() : now(),
    live: market.some((asset) => asset.metadata?.freshness === 'live'),
    fallback: fallback > 0 || nativeFreshness === 'fallback',
    delayed: market.some((asset) => asset.metadata?.delayed),
    coreAvailable: Boolean(nativeCore?.ok),
    status: nativeCore?.status ?? 'AuralithCore executable unavailable; using web bridge fallback contract.'
  };
}

function buildWatchlists(market: MarketAsset[], signals: Signal[]): CoreWatchlistGroup[] {
  const signalMap = new Map(signals.map((signal) => [signal.symbol, signal]));
  const groups: Array<[string, string, string, MarketAsset[]]> = [
    ['core-indices', 'Core Indices', 'Market ribbon and regime context', market.filter((asset) => ['SPY', 'QQQ', 'DIA', 'VIX'].includes(asset.symbol))],
    ['scanner-leaders', 'Scanner Leaders', 'Highest conviction research queue', [...market].sort((a, b) => (signalMap.get(b.symbol)?.score ?? 0) - (signalMap.get(a.symbol)?.score ?? 0)).slice(0, 8)],
    ['watchlist-movers', 'Watchlist Movers', 'Largest absolute moves', [...market].sort((a, b) => Math.abs(b.changePct) - Math.abs(a.changePct)).slice(0, 8)],
    ['risk-watch', 'Risk Watch', 'High volatility or stale/fallback data', market.filter((asset) => asset.volatility >= 0.045 || asset.metadata?.fallback || asset.metadata?.freshness === 'stale').slice(0, 8)]
  ];
  return groups.map(([id, name, role, assets]) => ({
    id,
    name,
    role,
    symbols: assets.map((asset) => asset.symbol),
    movers: assets.map((asset) => ({
      symbol: asset.symbol,
      changePct: asset.changePct,
      price: asset.price,
      freshness: asset.metadata?.freshness ?? 'fallback'
    }))
  }));
}

function buildPortfolioSnapshot(account: AppState['account'], positions: Position[], settings: RiskSettings): CorePortfolioSnapshot {
  const concentration = positions
    .map((position) => ({
      symbol: position.symbol,
      weight: account.equity ? position.marketValue / account.equity : 0,
      value: position.marketValue
    }))
    .sort((a, b) => b.weight - a.weight);
  const exposurePct = account.equity ? account.exposure / account.equity : 0;
  const warnings = [
    exposurePct > 0.72 ? 'Exposure is above the preferred monitoring threshold.' : '',
    concentration.some((item) => item.weight > settings.maxPositionPct) ? 'One or more positions exceed max position preference.' : '',
    account.dailyPnlPct < -settings.dailyLossLimitPct * 0.75 ? 'Daily loss pressure is approaching guardrail limits.' : ''
  ].filter(Boolean);
  return {
    equity: account.equity,
    cash: account.cash,
    exposure: account.exposure,
    realizedPnl: account.realizedPnl,
    unrealizedPnl: account.unrealizedPnl,
    dailyPnl: account.dailyPnl,
    dailyPnlPct: account.dailyPnlPct,
    riskState: riskState(account, settings),
    concentration,
    warnings
  };
}

function buildAlerts(audit: AuditEvent[]): CoreAlertItem[] {
  return audit
    .filter((event) => event.level !== 'info' || /alert|stale|provider|order|risk/i.test(event.eventType))
    .slice(0, 30)
    .map((event) => ({
      id: String(event.id),
      severity: event.level,
      source: 'AuralithCore audit bridge',
      timestamp: event.createdAt,
      relatedSymbol: extractSymbol(event.message),
      title: titleFromEvent(event.eventType),
      detail: event.message,
      acknowledged: false
    }));
}

function buildReports(state: Omit<AppState, 'core'>, provider: CoreProviderStatus): CoreReportSummary[] {
  const generatedAt = state.generatedAt;
  const portfolioRisk = riskState(state.account, state.settings);
  const topSignal = [...state.signals].sort((a, b) => b.score - a.score)[0];
  const staleCount = state.market.filter((asset) => asset.metadata?.freshness === 'stale' || asset.metadata?.fallback).length;
  const exposurePct = state.account.equity ? state.account.exposure / state.account.equity : 0;
  return [
    { id: 'morning-briefing', title: 'Morning Briefing', kind: 'morning', status: 'ready', generatedAt, source: 'AuralithCore reports contract', summary: `${topSignal ? `${topSignal.symbol} leads the research queue` : 'No dominant scanner leader yet'}; portfolio risk is ${portfolioRisk}; data freshness is ${provider.freshness}.` },
    { id: 'eod-review', title: 'End-of-Day Review', kind: 'eod', status: 'ready', generatedAt, source: 'AuralithCore reports contract', summary: `${money(state.account.dailyPnl)} paper daily P/L, ${state.orders.length} recent paper orders, and ${portfolioRisk} risk state are ready for close review.` },
    { id: 'weekly-review', title: 'Weekly Portfolio Review', kind: 'weekly', status: state.positions.length ? 'draft' : 'needs-data', generatedAt, source: 'AuralithCore reports contract', summary: `${state.positions.length} holdings, ${percent(exposurePct)} simulated exposure, ${percent(state.account.maxDrawdownPct)} drawdown, and ${money(state.account.cash)} cash require review.` },
    { id: 'risk-report', title: 'Risk Report', kind: 'risk', status: 'ready', generatedAt, source: 'AuralithCore reports contract', summary: `Risk state is ${portfolioRisk}; ${staleCount} symbols have stale or fallback context; paper-only execution remains locked.` },
    { id: 'strategy-report', title: 'Strategy Report', kind: 'strategy', status: state.signals.length ? 'draft' : 'needs-data', generatedAt, source: 'AuralithCore reports contract', summary: `${state.signals.length} scanner outputs are ready for paper strategy review with manual confirmation still required.` },
    { id: 'symbol-report', title: 'Symbol Research Pack', kind: 'symbol', status: state.market.length ? 'draft' : 'needs-data', generatedAt, source: 'AuralithCore reports contract', summary: 'Symbol reports combine quote freshness, scanner context, paper plan, notes, and SEC links from shared contracts.' }
  ];
}

function buildJournal(audit: AuditEvent[], orders: OrderRecord[]): CoreJournalEntry[] {
  const orderNotes = orders.slice(0, 12).map((order) => ({
    id: `order-${order.id}`,
    symbol: order.symbol,
    createdAt: order.createdAt,
    tag: 'paper-order',
    grade: order.status === 'filled' ? 'Review' : 'Risk',
    note: `${order.side.toUpperCase()} ${order.symbol} ${order.status}. ${order.reason}`,
    source: 'AuralithCore paper order bridge'
  }));
  const auditNotes = audit
    .filter((event) => /journal|note|lesson|mistake|strategy|risk|order/i.test(event.eventType + event.message))
    .slice(0, 12)
    .map((event) => ({
      id: `audit-${event.id}`,
      symbol: extractSymbol(event.message),
      createdAt: event.createdAt,
      tag: event.eventType,
      note: event.message,
      source: 'AuralithCore audit bridge'
    }));
  return [...orderNotes, ...auditNotes];
}

function buildAiSummaries(state: Omit<AppState, 'core'>, provider: CoreProviderStatus): CoreAiSummary[] {
  const topSignal = [...state.signals].sort((a, b) => b.score - a.score)[0];
  const risk = riskState(state.account, state.settings);
  const exposurePct = state.account.equity ? state.account.exposure / state.account.equity : 0;
  const staleCount = state.market.filter((asset) => asset.metadata?.freshness === 'stale').length;
  const fallbackCount = state.market.filter((asset) => asset.metadata?.fallback).length;
  const concentrated = state.positions
    .map((position) => ({ symbol: position.symbol, weight: state.account.equity ? position.marketValue / state.account.equity : 0 }))
    .sort((a, b) => b.weight - a.weight)[0];
  return [
    {
      id: 'market-brief',
      scope: 'market',
      title: 'Market Briefing',
      summary: topSignal
        ? `${topSignal.symbol} leads the scanner because momentum, risk/reward, and confirmation inputs currently rank above the rest of the universe. This is a paper setup queue, not a forecast.`
        : 'Scanner output is limited until market data refreshes; research confidence should stay conservative.',
      confidence: provider.confidence / 100,
      evidence: [`Provider: ${provider.source}`, `Freshness: ${provider.freshness}`, `Signals: ${state.signals.length}`, `Fallback symbols: ${fallbackCount}`],
      source: 'AuralithCore AI summary contract',
      freshness: provider.freshness
    },
    {
      id: 'portfolio-risk',
      scope: 'portfolio',
      title: 'Portfolio Risk',
      summary: `Portfolio risk appears ${risk.toLowerCase()} with ${percent(exposurePct)} simulated exposure${concentrated ? ` and ${concentrated.symbol} as the largest weight` : ''}. Review concentration before adding new paper risk.`,
      confidence: Math.min(0.9, Math.max(0.45, provider.confidence / 100 - (fallbackCount ? 0.12 : 0))),
      evidence: [`Exposure: ${money(state.account.exposure)}`, `Cash: ${money(state.account.cash)}`, `Daily P/L: ${money(state.account.dailyPnl)}`, concentrated ? `Largest weight: ${concentrated.symbol} ${percent(concentrated.weight)}` : 'No open positions'],
      source: 'AuralithCore AI summary contract',
      freshness: provider.freshness
    },
    {
      id: 'data-quality',
      scope: 'risk',
      title: 'Data Quality',
      summary: provider.fallback || staleCount
        ? `Research confidence is reduced because ${fallbackCount} symbols are using fallback context and ${staleCount} are stale.`
        : 'Provider metadata is currently usable for research workflows; continue checking freshness before relying on signal context.',
      confidence: provider.confidence / 100,
      evidence: [`Quote count: ${provider.quoteCount}`, `Confidence: ${provider.confidence}%`, `Delayed: ${provider.delayed ? 'yes' : 'no'}`],
      source: 'AuralithCore provider contract',
      freshness: provider.freshness
    }
  ];
}

function buildSymbolAiSummary(symbol: string, quote: MarketAsset | undefined, signal: Signal | undefined, freshness: DataFreshness): CoreAiSummary {
  const confidence = (quote?.metadata?.confidence ?? 45) / 100;
  const dataQualifier = freshness === 'live' ? 'fresh provider context' : `${freshness} data context`;
  return {
    id: `symbol-${symbol}`,
    scope: 'symbol',
    title: `${symbol} Research Context`,
    summary: signal
      ? `${signal.setupType} is based on momentum score ${Math.round(signal.momentumScore)}, ${dataQualifier}, and ${signal.riskReward.toFixed(1)}R paper risk/reward. The sell zone is a planning range, not a prediction.`
      : quote
        ? `${symbol} has quote context, but no current paper setup meets scanner requirements. Momentum or risk confirmation is incomplete.`
        : `${symbol} is not currently in the shared market universe.`,
    confidence,
    evidence: [
      `Freshness: ${freshness}`,
      signal ? `Risk/reward: ${signal.riskReward.toFixed(1)}R` : 'No scanner setup',
      signal ? `Invalidation: ${signal.invalidation}` : 'Confirmation not met',
      quote ? `Provider: ${quote.metadata?.provider ?? quote.source}` : 'No quote'
    ],
    source: 'AuralithCore AI summary contract',
    freshness
  };
}

function buildTrackedSymbolResearch(
  state: Omit<AppState, 'core'>,
  alerts: CoreAlertItem[],
  journal: CoreJournalEntry[]
) {
  return prioritizedSymbols(state)
    .map((symbol) => buildSymbolResearch(symbol, state, alerts, journal))
    .filter((research) => research.quote || research.signal || research.alerts.length || research.journalNotes.length);
}

function prioritizedSymbols(state: Omit<AppState, 'core'>) {
  const symbols = new Set<string>();
  state.positions.forEach((position) => symbols.add(position.symbol));
  state.orders.slice(0, 20).forEach((order) => symbols.add(order.symbol));
  [...state.signals].sort((a, b) => b.score - a.score).slice(0, 40).forEach((signal) => symbols.add(signal.symbol));
  state.market
    .filter((asset) => ['SPY', 'QQQ', 'DIA', 'VIX'].includes(asset.symbol))
    .forEach((asset) => symbols.add(asset.symbol));
  state.market
    .filter((asset) => asset.metadata?.fallback || asset.metadata?.freshness === 'stale' || asset.volatility >= 0.045)
    .slice(0, 30)
    .forEach((asset) => symbols.add(asset.symbol));
  [...state.market]
    .sort((a, b) => Math.abs(b.changePct) - Math.abs(a.changePct))
    .slice(0, 30)
    .forEach((asset) => symbols.add(asset.symbol));
  return [...symbols].slice(0, 96);
}

function buildTrustSnapshot(state: AppState) {
  const counts = state.market.reduce(
    (acc, asset) => {
      const freshness = asset.metadata?.freshness ?? 'fallback';
      acc[freshness] += 1;
      if (asset.metadata?.fallback) acc.fallback += freshness === 'fallback' ? 0 : 1;
      return acc;
    },
    { live: 0, delayed: 0, stale: 0, fallback: 0, failed: 0 } as Record<DataFreshness, number>
  );
  return {
    dataQuality: counts,
    averageConfidence: Math.round(average(state.market.map((asset) => asset.metadata?.confidence ?? 50))),
    auditEvents: state.audit.length,
    lastProviderRefresh: state.core.providerHealth.lastRefresh
  };
}

function latestTimestamp(market: MarketAsset[]) {
  const timestamps = market.map((asset) => Date.parse(asset.metadata?.fetchedAt ?? asset.updatedAt)).filter(Number.isFinite);
  return timestamps.length ? Math.max(...timestamps) : 0;
}

function uniqueAsset(asset: MarketAsset, index: number, assets: MarketAsset[]) {
  return assets.findIndex((item) => item.symbol === asset.symbol) === index;
}

function providerLabel(market: MarketAsset[]) {
  const providers = [...new Set(market.map((asset) => asset.metadata?.provider ?? asset.source).filter(Boolean))];
  if (!providers.length) return 'No provider';
  if (providers.length === 1) return providers[0] ?? 'No provider';
  return `${providers[0]} + ${providers.length - 1} more`;
}

function freshnessFromAge(timestamp: number, fallback: boolean): DataFreshness {
  if (fallback) return 'fallback';
  if (!timestamp) return 'failed';
  const age = Date.now() - timestamp;
  if (age < 2 * 60 * 1000) return 'live';
  if (age < 20 * 60 * 1000) return 'delayed';
  return 'stale';
}

function normalizeFreshness(value?: string): DataFreshness | null {
  const lower = value?.toLowerCase();
  if (lower === 'live' || lower === 'delayed' || lower === 'stale' || lower === 'fallback' || lower === 'failed') return lower;
  if (lower === 'error') return 'failed';
  return null;
}

function average(values: number[]) {
  if (!values.length) return 0;
  return values.reduce((sum, value) => sum + value, 0) / values.length;
}

function riskState(account: AppState['account'], settings: RiskSettings): CorePortfolioSnapshot['riskState'] {
  if (account.maxDrawdownPct < -0.14 || account.dailyPnlPct < -settings.dailyLossLimitPct) return 'Critical';
  if (account.maxDrawdownPct < -0.1 || account.dailyPnlPct < -settings.dailyLossLimitPct * 0.85) return 'Defensive';
  if (account.maxDrawdownPct < -0.08 || account.dailyPnlPct < -settings.dailyLossLimitPct * 0.75) return 'Elevated';
  if (account.exposure / Math.max(account.equity, 1) > 0.72) return 'Caution';
  return 'Healthy';
}

function extractSymbol(text: string) {
  const match = text.match(/\b[A-Z]{1,5}(?:[.-][A-Z])?\b/);
  return match?.[0];
}

function titleFromEvent(eventType: string) {
  return eventType
    .split(/[._-]/)
    .filter(Boolean)
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(' ');
}

function compact(value: number) {
  return new Intl.NumberFormat('en-US', { notation: 'compact', maximumFractionDigits: 1 }).format(value);
}

function money(value: number) {
  return new Intl.NumberFormat('en-US', { style: 'currency', currency: 'USD', maximumFractionDigits: 0 }).format(value);
}

function percent(value: number) {
  return `${(value * 100).toFixed(2)}%`;
}
