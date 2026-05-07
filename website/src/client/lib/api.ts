import type {
  AlertHistoryRecord,
  AlertRecord,
  AppState,
  AuthResult,
  AuthSession,
  AuthUser,
  BackupValidationResult,
  CandleRecord,
  DataQualityResponse,
  JournalRecord,
  NotificationRecord,
  OrderSide,
  ProviderDiagnosticsResponse,
  PlatformHealthSnapshot,
  ResearchTimelineEvent,
  ResearchWorkflow,
  RiskSettings,
  SavedReport,
  StrategyPerformance,
  StrategyRecord,
  WatchlistRecord,
  WorkspaceLayoutRecord
} from '../types/api';

const LEGACY_AUTH_TOKEN_STORAGE_KEY = 'auralith:web:auth-token:v1';
let csrfToken = '';

async function jsonFetch<T>(path: string, init?: RequestInit): Promise<T> {
  dropLegacyAuthToken();
  const method = (init?.method || 'GET').toUpperCase();
  const headers = new Headers(init?.headers);
  const isMutation = !['GET', 'HEAD', 'OPTIONS'].includes(method);
  if (!headers.has('Content-Type') && init?.body) headers.set('Content-Type', 'application/json');
  if (isMutation) headers.set('X-CSRF-Token', await getCsrfToken());
  const response = await fetch(path, {
    ...init,
    credentials: 'include',
    headers
  });
  if (!response.ok) {
    const text = await response.text();
    let message = text;
    try {
      const body = JSON.parse(text) as { error?: string; message?: string };
      message = body.error || body.message || text;
    } catch {
      // Keep the raw response text when the server did not return JSON.
    }
    throw new Error(message || `${response.status} ${response.statusText}`);
  }
  return response.json() as Promise<T>;
}

async function getCsrfToken() {
  if (csrfToken) return csrfToken;
  const response = await fetch('/api/auth/csrf', { credentials: 'include' });
  if (!response.ok) throw new Error('Could not initialize Auralith security token.');
  const body = await response.json() as { ok: true; csrfToken: string };
  csrfToken = body.csrfToken;
  return csrfToken;
}

export function getState() {
  return jsonFetch<AppState>('/api/state');
}

export function runCycle() {
  return jsonFetch<AppState>('/api/tick', { method: 'POST' });
}

export function addMarketSymbol(symbol: string) {
  return jsonFetch<AppState>('/api/market/quote', { method: 'POST', body: JSON.stringify({ symbol }) });
}

export function updateSettings(settings: Partial<RiskSettings>) {
  return jsonFetch<AppState>('/api/settings', { method: 'POST', body: JSON.stringify(settings) });
}

export function placeOrder(symbol: string, side: OrderSide, qty: number, signal: string) {
  return jsonFetch<AppState>('/api/orders', {
    method: 'POST',
    body: JSON.stringify({ symbol, side, qty, signal, manualConfirmed: true })
  });
}

export function resetPaperAccount() {
  return jsonFetch<AppState>('/api/reset', { method: 'POST' });
}

export function runBacktest() {
  return jsonFetch<any>('/api/backtest');
}

export function listAlerts() {
  return jsonFetch<{ ok: true; alerts: AlertRecord[]; history: AlertHistoryRecord[] }>('/api/alerts');
}

export function createAlert(input: Partial<AlertRecord>) {
  return jsonFetch<{ ok: true; alert: AlertRecord }>('/api/alerts', { method: 'POST', body: JSON.stringify(input) });
}

export function updateAlert(id: string, input: Partial<AlertRecord>) {
  return jsonFetch<{ ok: true; alert: AlertRecord }>(`/api/alerts/${encodeURIComponent(id)}`, { method: 'PATCH', body: JSON.stringify(input) });
}

export function deleteAlert(id: string) {
  return jsonFetch<{ ok: true }>(`/api/alerts/${encodeURIComponent(id)}`, { method: 'DELETE' });
}

export function acknowledgeAlert(id: string) {
  return jsonFetch<{ ok: true; alert: AlertRecord | null }>(`/api/alerts/${encodeURIComponent(id)}/acknowledge`, { method: 'POST' });
}

export function snoozeAlert(id: string, minutes = 60) {
  return jsonFetch<{ ok: true; alert: AlertRecord | null }>(`/api/alerts/${encodeURIComponent(id)}/snooze`, {
    method: 'POST',
    body: JSON.stringify({ minutes })
  });
}

export function listJournal(filters: { symbol?: string; tag?: string } = {}) {
  const params = new URLSearchParams();
  if (filters.symbol) params.set('symbol', filters.symbol);
  if (filters.tag) params.set('tag', filters.tag);
  const query = params.toString();
  return jsonFetch<{ ok: true; entries: JournalRecord[] }>(`/api/journal${query ? `?${query}` : ''}`);
}

export function createJournalEntry(input: Partial<JournalRecord>) {
  return jsonFetch<{ ok: true; entry: JournalRecord }>('/api/journal', { method: 'POST', body: JSON.stringify(input) });
}

export function updateJournalEntry(id: string, input: Partial<JournalRecord>) {
  return jsonFetch<{ ok: true; entry: JournalRecord }>(`/api/journal/${encodeURIComponent(id)}`, { method: 'PATCH', body: JSON.stringify(input) });
}

export function deleteJournalEntry(id: string) {
  return jsonFetch<{ ok: true }>(`/api/journal/${encodeURIComponent(id)}`, { method: 'DELETE' });
}

export function listReports() {
  return jsonFetch<{ ok: true; reports: SavedReport[] }>('/api/reports');
}

export function generateReport(kind: SavedReport['kind'], symbol?: string) {
  return jsonFetch<{ ok: true; report: SavedReport }>('/api/reports', { method: 'POST', body: JSON.stringify({ kind, symbol }) });
}

export function getCandles(symbol: string, timeframe = '1d') {
  return jsonFetch<{ ok: true; candles: CandleRecord[] }>(`/api/candles/${encodeURIComponent(symbol)}?timeframe=${encodeURIComponent(timeframe)}`);
}

export function getProviderDiagnostics() {
  return jsonFetch<ProviderDiagnosticsResponse>('/api/providers/diagnostics');
}

export function getDataQuality() {
  return jsonFetch<DataQualityResponse>('/api/data-quality');
}

export function getMorningBriefing() {
  return jsonFetch<{ ok: true; workflow: ResearchWorkflow }>('/api/workflows/morning');
}

export function getEndOfDayReview() {
  return jsonFetch<{ ok: true; workflow: ResearchWorkflow }>('/api/workflows/eod');
}

export function getResearchTimeline(symbol: string) {
  return jsonFetch<{ ok: true; symbol: string; timeline: ResearchTimelineEvent[] }>(`/api/research/timeline/${encodeURIComponent(symbol)}`);
}

export function getPlatformHealth() {
  return jsonFetch<{ ok: true; health: PlatformHealthSnapshot }>('/api/platform/health');
}

export function validatePlatformBackup() {
  return jsonFetch<{ ok: boolean; backup: BackupValidationResult }>('/api/platform/backup/validate', { method: 'POST' });
}

export function listWatchlists() {
  return jsonFetch<{ ok: true; watchlists: WatchlistRecord[] }>('/api/watchlists');
}

export function createWatchlist(input: Partial<WatchlistRecord>) {
  return jsonFetch<{ ok: true; watchlist: WatchlistRecord }>('/api/watchlists', { method: 'POST', body: JSON.stringify(input) });
}

export function updateWatchlist(id: string, input: Partial<WatchlistRecord>) {
  return jsonFetch<{ ok: true; watchlist: WatchlistRecord }>(`/api/watchlists/${encodeURIComponent(id)}`, { method: 'PATCH', body: JSON.stringify(input) });
}

export function deleteWatchlist(id: string) {
  return jsonFetch<{ ok: true }>(`/api/watchlists/${encodeURIComponent(id)}`, { method: 'DELETE' });
}

export function listStrategies() {
  return jsonFetch<{ ok: true; strategies: StrategyRecord[]; performance: StrategyPerformance[] }>('/api/strategies');
}

export function saveStrategy(input: Partial<StrategyRecord>) {
  return jsonFetch<{ ok: true; strategy: StrategyRecord }>('/api/strategies', { method: 'POST', body: JSON.stringify(input) });
}

export function deleteStrategy(id: string) {
  return jsonFetch<{ ok: true }>(`/api/strategies/${encodeURIComponent(id)}`, { method: 'DELETE' });
}

export function listLayouts() {
  return jsonFetch<{ ok: true; layouts: WorkspaceLayoutRecord[] }>('/api/layouts');
}

export function saveLayout(input: Partial<WorkspaceLayoutRecord>) {
  return jsonFetch<{ ok: true; layout: WorkspaceLayoutRecord }>('/api/layouts', { method: 'POST', body: JSON.stringify(input) });
}

export function listNotifications() {
  return jsonFetch<{ ok: true; notifications: NotificationRecord[] }>('/api/notifications');
}

export function markNotificationRead(id: string) {
  return jsonFetch<{ ok: true }>(`/api/notifications/${encodeURIComponent(id)}/read`, { method: 'POST' });
}

export function updatePaperCash(cash: number) {
  return jsonFetch<AppState>('/api/portfolio/cash', { method: 'POST', body: JSON.stringify({ cash }) });
}

export function getAuthToken() {
  return '';
}

export function setAuthToken(token: string) {
  void token;
}

export function clearAuthToken() {
  csrfToken = '';
  dropLegacyAuthToken();
}

function dropLegacyAuthToken() {
  try {
    window.localStorage.removeItem(LEGACY_AUTH_TOKEN_STORAGE_KEY);
  } catch {
    // Ignore locked-down browser storage; active sessions are cookie-backed now.
  }
}

export async function registerAccount(input: { email: string; password: string; workspaceName: string; rememberDevice?: boolean }) {
  const result = await jsonFetch<{ ok: true } & AuthResult>('/api/auth/register', {
    method: 'POST',
    body: JSON.stringify(input)
  });
  return result;
}

export async function loginAccount(input: { email: string; password: string; rememberDevice?: boolean }) {
  const result = await jsonFetch<{ ok: true } & AuthResult>('/api/auth/login', {
    method: 'POST',
    body: JSON.stringify(input)
  });
  return result;
}

export function currentAccount() {
  return jsonFetch<{ ok: true; user: AuthUser }>('/api/auth/session');
}

export function accountSessions() {
  return jsonFetch<{ ok: true; sessions: AuthSession[] }>('/api/auth/sessions');
}

export async function logoutAccount() {
  try {
    await jsonFetch<{ ok: true }>('/api/auth/logout', { method: 'POST' });
  } finally {
    clearAuthToken();
  }
}

export async function logoutAllAccountSessions() {
  try {
    await jsonFetch<{ ok: true }>('/api/auth/logout-all', { method: 'POST' });
  } finally {
    clearAuthToken();
  }
}
