export type AssetClass = 'crypto' | 'stock';
export type OrderSide = 'buy' | 'sell';
export type MarketRegime = 'trending' | 'sideways' | 'volatile' | 'unknown';
export type DataFreshness = 'live' | 'delayed' | 'stale' | 'fallback' | 'failed';

export interface MarketDataMetadata {
  provider: string;
  fetchedAt: string;
  freshness: DataFreshness;
  delayed: boolean;
  fallback: boolean;
  confidence: number;
  error?: string;
}

export interface MarketAsset {
  symbol: string;
  name: string;
  assetClass: AssetClass;
  price: number;
  previousClose: number;
  changePct: number;
  momentum: number;
  volatility: number;
  volume: number;
  source: string;
  marketCap?: number;
  updatedAt: string;
  metadata?: MarketDataMetadata;
}

export interface Position {
  symbol: string;
  assetClass: AssetClass;
  qty: number;
  avgPrice: number;
  lastPrice: number;
  marketValue: number;
  unrealizedPnl: number;
  unrealizedPnlPct: number;
  entryTime: number;
}

export interface OrderRecord {
  id: string;
  createdAt: string;
  symbol: string;
  side: OrderSide;
  qty: number;
  status: 'filled' | 'rejected' | 'pending';
  mode: 'paper' | 'live';
  price: number;
  notional: number;
  reason: string;
  signal: string;
}

export interface AccountSummary {
  cash: number;
  equity: number;
  startingEquity: number;
  realizedPnl: number;
  unrealizedPnl: number;
  exposure: number;
  dailyPnl: number;
  dailyPnlPct: number;
  maxDrawdownPct: number;
}

export interface RiskSettings {
  mode: 'paper' | 'live';
  liveTradingLocked: boolean;
  manualConfirmationRequired: boolean;
  autopilot: boolean;
  maxPositionPct: number;
  maxTradeNotional: number;
  maxOpenPositions: number;
  dailyLossLimitPct: number;
  minConfidence: number;
  disabledStrategies: string[];
}

export interface Signal {
  symbol: string;
  assetClass: AssetClass;
  currentPrice: number;
  side: OrderSide | 'hold';
  confidence: number;
  score: number;
  setupType: string;
  entry: number;
  stop: number;
  target1: number;
  target2: number;
  target: number;
  trailingStop: number;
  resistance: number | null;
  atr: number;
  expectedSellZone: string;
  exitPlan: string;
  invalidation: string;
  riskReward: number;
  riskScore: number;
  momentumScore: number;
  manualConfirmationRequired: boolean;
  paperOnly: boolean;
  components: {
    trend: number;
    volume: number;
    oscillator: number;
    breakout: number;
    sector: number;
    regime: number;
  };
  reason: string;
  risk: string;
}

export interface AuditEvent {
  id: number;
  createdAt: string;
  level: 'info' | 'warning' | 'error';
  eventType: string;
  message: string;
}

export interface StrategyPerformance {
  strategy: string;
  total_trades: number;
  winning_trades: number;
  total_pnl: number;
  avg_pnl: number;
  win_rate: number;
  last_updated: string;
}

export interface AuthUser {
  id: string;
  email: string;
  workspaceName: string;
  createdAt: string;
  lastLoginAt?: string;
}

export interface AuthSession {
  id: string;
  userId: string;
  deviceLabel: string;
  createdAt: string;
  expiresAt: string;
  revokedAt?: string;
}

export interface AuthResult {
  user: AuthUser;
  session: AuthSession;
}

export interface CoreProviderStatus {
  source: string;
  freshness: DataFreshness;
  quoteCount: number;
  confidence: number;
  lastRefresh: string;
  live: boolean;
  fallback: boolean;
  delayed: boolean;
  coreAvailable: boolean;
  status: string;
}

export interface CoreWatchlistGroup {
  id: string;
  name: string;
  role: string;
  symbols: string[];
  movers: Array<{ symbol: string; changePct: number; price: number; freshness: DataFreshness }>;
}

export interface CorePortfolioSnapshot {
  equity: number;
  cash: number;
  exposure: number;
  realizedPnl: number;
  unrealizedPnl: number;
  dailyPnl: number;
  dailyPnlPct: number;
  riskState: 'Healthy' | 'Caution' | 'Elevated' | 'Defensive' | 'Critical';
  concentration: Array<{ symbol: string; weight: number; value: number }>;
  warnings: string[];
}

export interface CoreAlertItem {
  id: string;
  severity: 'info' | 'warning' | 'error';
  source: string;
  timestamp: string;
  relatedSymbol?: string;
  title: string;
  detail: string;
  acknowledged: boolean;
  snoozedUntil?: string;
}

export interface CoreReportSummary {
  id: string;
  title: string;
  kind: 'morning' | 'eod' | 'weekly' | 'risk' | 'strategy' | 'symbol';
  status: 'ready' | 'draft' | 'needs-data';
  generatedAt: string;
  source: string;
  summary: string;
}

export interface CoreJournalEntry {
  id: string;
  symbol?: string;
  createdAt: string;
  tag: string;
  grade?: string;
  note: string;
  source: string;
}

export interface CoreAiSummary {
  id: string;
  scope: 'market' | 'portfolio' | 'watchlist' | 'symbol' | 'risk';
  title: string;
  summary: string;
  confidence: number;
  evidence: string[];
  source: string;
  freshness: DataFreshness;
}

export interface CoreSymbolResearch {
  symbol: string;
  quote?: MarketAsset;
  signal?: Signal;
  quoteFreshness: DataFreshness;
  provider: string;
  momentumScore: number;
  fundamentals: Array<{ label: string; value: string; source: string }>;
  earnings: Array<{ label: string; value: string; source: string }>;
  news: Array<{ title: string; source: string; freshness: DataFreshness }>;
  secLinks: Array<{ title: string; url: string; source: string }>;
  alerts: CoreAlertItem[];
  journalNotes: CoreJournalEntry[];
  paperTradePlan?: {
    setupType: string;
    currentPrice: number;
    entry: number;
    stop: number;
    target1: number;
    target2: number;
    trailingStop: number;
    expectedSellZone: string;
    invalidation: string;
    exitPlan: string;
  };
  aiExplanation: CoreAiSummary;
}

export interface CoreContracts {
  contractVersion: 'auralith-core-web-v1';
  generatedAt: string;
  platform: {
    desktopPrimary: boolean;
    webRole: string;
    sharedCore: string;
    nextExtraction: string;
  };
  safety: {
    paperOnly: boolean;
    manualConfirmationRequired: boolean;
    liveTradingLocked: boolean;
    advisoryNotice: string;
  };
  providerHealth: CoreProviderStatus;
  watchlists: CoreWatchlistGroup[];
  scanner: Signal[];
  portfolio: CorePortfolioSnapshot;
  alerts: CoreAlertItem[];
  reports: CoreReportSummary[];
  journal: CoreJournalEntry[];
  symbolResearch: CoreSymbolResearch[];
  paperOrders: OrderRecord[];
  aiSummaries: CoreAiSummary[];
}

export interface CoreApiResponse<T> {
  ok: true;
  contractVersion: 'auralith-core-web-v1';
  source: 'AuralithCore';
  generatedAt: string;
  data: T;
}

export type AlertType = 'price_above' | 'price_below' | 'percent_move' | 'provider_stale' | 'risk_threshold' | 'paper_order_update';
export type AlertSeverity = 'info' | 'warning' | 'error';

export interface AlertRecord {
  id: string;
  type: AlertType;
  symbol?: string;
  threshold?: number;
  percentMove?: number;
  severity: AlertSeverity;
  enabled: boolean;
  acknowledgedAt?: string;
  snoozedUntil?: string;
  lastTriggeredAt?: string;
  note: string;
  createdAt: string;
  updatedAt: string;
}

export interface AlertHistoryRecord {
  id: string;
  alertId?: string;
  createdAt: string;
  severity: AlertSeverity;
  symbol?: string;
  title: string;
  detail: string;
  acknowledgedAt?: string;
}

export interface JournalRecord {
  id: string;
  symbol?: string;
  entryType: 'note' | 'trade_review' | 'lesson' | 'mistake' | 'research';
  tags: string[];
  note: string;
  mistake?: string;
  lesson?: string;
  grade?: string;
  realizedPnl?: number;
  createdAt: string;
  updatedAt: string;
}

export interface SavedReport {
  id: string;
  kind: 'morning' | 'eod' | 'weekly' | 'risk' | 'strategy' | 'symbol';
  title: string;
  status: 'ready' | 'draft' | 'needs-data';
  source: string;
  freshness: DataFreshness;
  summary: string;
  html: string;
  payload: unknown;
  createdAt: string;
  updatedAt: string;
}

export interface CandleRecord {
  symbol: string;
  assetType: AssetClass;
  timeframe: string;
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  provider: string;
  fetchedAt: string;
  freshness: DataFreshness;
  fallback: boolean;
  synthetic: boolean;
}

export interface WatchlistRecord {
  id: string;
  name: string;
  role: string;
  tags: string[];
  symbols: string[];
  createdAt: string;
  updatedAt: string;
}

export interface StrategyRecord {
  id: string;
  name: string;
  description: string;
  rules: string[];
  preset: string;
  enabled: boolean;
  createdAt: string;
  updatedAt: string;
}

export interface WorkspaceLayoutRecord {
  id: string;
  name: string;
  mode: 'daily' | 'market-open' | 'research' | 'portfolio-review' | 'strategy-testing' | 'mobile-compact';
  config: unknown;
  createdAt: string;
  updatedAt: string;
}

export interface NotificationRecord {
  id: string;
  severity: AlertSeverity;
  source: string;
  title: string;
  detail: string;
  relatedSymbol?: string;
  readAt?: string;
  snoozedUntil?: string;
  createdAt: string;
}

export type ProviderCapability =
  | 'quotes'
  | 'candles'
  | 'fundamentals'
  | 'news'
  | 'earnings'
  | 'sec-filings'
  | 'etf-exposure'
  | 'crypto'
  | 'macro';

export interface ProviderDiagnostics {
  id: string;
  name: string;
  tier: 'primary' | 'secondary' | 'public-fallback' | 'demo-fallback';
  capabilities: ProviderCapability[];
  status: 'ready' | 'degraded' | 'failed' | 'disabled';
  lastAttemptAt?: string;
  lastSuccessAt?: string;
  lastError?: string;
  requestCount: number;
  failureCount: number;
  cacheHits: number;
  fallbackCount: number;
  averageLatencyMs: number;
  rateLimitState: 'ok' | 'limited' | 'unknown';
}

export interface DataQualitySnapshot {
  counts: Record<DataFreshness, number>;
  confidence: number;
  warnings: string[];
  generatedAt: string;
}

export interface ProviderDiagnosticsResponse {
  ok: true;
  diagnostics: ProviderDiagnostics[];
  dataQuality: DataQualitySnapshot;
  generatedAt: string;
}

export interface DataQualityResponse {
  ok: true;
  dataQuality: DataQualitySnapshot;
  quotes: Array<{
    symbol: string;
    assetClass: AssetClass;
    provider: string;
    fetchedAt: string;
    freshness: DataFreshness;
    confidence: number;
    delayed: boolean;
    fallback: boolean;
    error?: string;
  }>;
  generatedAt: string;
}

export type WorkflowSeverity = 'positive' | 'info' | 'warning' | 'critical';

export interface ResearchWorkflowItem {
  label: string;
  detail: string;
  severity: WorkflowSeverity;
  source: string;
  symbol?: string;
  freshness?: DataFreshness;
}

export interface ResearchWorkflowSection {
  id: string;
  title: string;
  summary: string;
  items: ResearchWorkflowItem[];
}

export interface ResearchWorkflow {
  id: string;
  title: string;
  summary: string;
  generatedAt: string;
  confidence: number;
  sections: ResearchWorkflowSection[];
  actionChecklist: ResearchWorkflowItem[];
  disclosure: string;
}

export interface ResearchTimelineEvent {
  id: string;
  timestamp: string;
  type: 'quote' | 'signal' | 'alert' | 'journal' | 'report' | 'paper-order' | 'risk' | 'provider';
  title: string;
  detail: string;
  severity: WorkflowSeverity;
  source: string;
  symbol?: string;
  freshness?: DataFreshness;
}

export interface PlatformHealthSnapshot {
  generatedAt: string;
  status: 'healthy' | 'degraded' | 'attention';
  safeMode: boolean;
  uptimeSeconds: number;
  database: {
    path: string;
    exists: boolean;
    sizeBytes: number;
    walMode: string;
    integrity: 'ok' | 'failed';
    integrityDetail: string;
    tableCount: number;
  };
  continuity: {
    quotes: number;
    journalEntries: number;
    reports: number;
    alerts: number;
    alertHistory: number;
    paperOrders: number;
    notifications: number;
  };
  providers: {
    total: number;
    failed: number;
    degraded: number;
    fallbackRequests: number;
    cacheHits: number;
  };
  trust: {
    confidence: number;
    fallbackSymbols: number;
    staleSymbols: number;
    failedSymbols: number;
    disclosure: string;
  };
  recommendations: string[];
}

export interface BackupValidationResult {
  ok: boolean;
  backupPath?: string;
  sizeBytes?: number;
  integrity?: string;
  createdAt: string;
  error?: string;
}

export interface AppState {
  account: AccountSummary;
  settings: RiskSettings;
  market: MarketAsset[];
  positions: Position[];
  orders: OrderRecord[];
  signals: Signal[];
  audit: AuditEvent[];
  regime: MarketRegime;
  strategyPerformance: StrategyPerformance[];
  core: CoreContracts;
  generatedAt: string;
}
