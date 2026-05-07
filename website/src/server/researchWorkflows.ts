import type {
  AlertHistoryRecord,
  AppState,
  DataFreshness,
  JournalRecord,
  MarketAsset,
  SavedReport,
  Signal,
  StrategyRecord
} from './types.js';

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

export interface ResearchWorkflowInputs {
  journal: JournalRecord[];
  alertHistory: AlertHistoryRecord[];
  reports: SavedReport[];
  strategies: StrategyRecord[];
}

const DISCLOSURE = 'Auralith is research-first and paper-first. These workflows organize context; they are not financial advice and do not place live orders.';

export function buildMorningBriefing(state: AppState, inputs: ResearchWorkflowInputs): ResearchWorkflow {
  const topMovers = [...state.market].sort((a, b) => Math.abs(b.changePct) - Math.abs(a.changePct)).slice(0, 6);
  const scannerLeaders = [...state.signals].sort((a, b) => b.score - a.score).slice(0, 5);
  const staleAssets = state.market.filter((asset) => asset.metadata?.fallback || asset.metadata?.freshness === 'stale' || asset.metadata?.freshness === 'failed').slice(0, 6);
  const portfolioWarnings = state.core.portfolio.warnings;
  const openPositions = state.positions.slice(0, 6);
  const recentAlerts = inputs.alertHistory.slice(0, 5);
  const recentJournal = inputs.journal.slice(0, 5);
  const confidence = briefingConfidence(state);
  const riskState = state.core.portfolio.riskState;
  const topSignal = scannerLeaders[0];

  return {
    id: 'morning-briefing',
    title: 'Morning Briefing',
    generatedAt: state.generatedAt,
    confidence,
    summary: topSignal
      ? `${topSignal.symbol} leads the paper research queue; portfolio risk is ${riskState}; data confidence is ${confidence}%.`
      : `No dominant scanner leader is available; portfolio risk is ${riskState}; data confidence is ${confidence}%.`,
    sections: [
      {
        id: 'market-overview',
        title: 'Market Overview',
        summary: `${topMovers.length} symbols show the largest current movement in the monitored universe.`,
        items: topMovers.map((asset) => ({
          label: `${asset.symbol} ${asset.changePct >= 0 ? 'advanced' : 'declined'} ${pct(asset.changePct)}`,
          detail: `${assetClassLabel(asset)} at ${money(asset.price)} with ${asset.metadata?.freshness ?? 'fallback'} data from ${assetProvider(asset)}.`,
          severity: Math.abs(asset.changePct) > 0.05 ? 'warning' : asset.changePct >= 0 ? 'positive' : 'info',
          source: assetProvider(asset),
          symbol: asset.symbol,
          freshness: asset.metadata?.freshness
        }))
      },
      {
        id: 'portfolio-risk',
        title: 'Portfolio Risk Check',
        summary: `${riskState} risk state with ${money(state.account.exposure)} simulated exposure and ${money(state.account.cash)} paper cash.`,
        items: portfolioWarnings.length
          ? portfolioWarnings.map((warning) => item('Portfolio warning', warning, 'warning', 'Risk engine'))
          : [item('No active portfolio warnings', 'Exposure and drawdown are within current paper risk preferences.', 'positive', 'Risk engine')]
      },
      {
        id: 'watchlist-scanner',
        title: 'Watchlist And Scanner Focus',
        summary: scannerLeaders.length ? `${scannerLeaders.length} scanner leaders are ready for paper review.` : 'Scanner context is limited until market data refreshes.',
        items: scannerLeaders.map((signal) => ({
          label: `${signal.symbol} ${signal.setupType}`,
          detail: buildSignalNarrative(signal, state.market.find((asset) => asset.symbol === signal.symbol), state),
          severity: signal.riskScore >= 70 ? 'warning' : signal.score >= 75 ? 'positive' : 'info',
          source: 'Scanner engine',
          symbol: signal.symbol,
          freshness: state.market.find((asset) => asset.symbol === signal.symbol)?.metadata?.freshness
        }))
      },
      {
        id: 'alerts-journal',
        title: 'Alerts And Journal Context',
        summary: `${recentAlerts.length} recent alert events and ${recentJournal.length} recent journal entries are available for review.`,
        items: [
          ...recentAlerts.map((alert) => ({
            label: alert.title,
            detail: alert.detail,
            severity: alert.severity === 'error' ? 'critical' as const : alert.severity === 'warning' ? 'warning' as const : 'info' as const,
            source: 'Alert history',
            symbol: alert.symbol
          })),
          ...recentJournal.map((entry) => ({
            label: `${entry.symbol ?? 'General'} journal note`,
            detail: summarizeText(entry.note, 170),
            severity: entry.mistake ? 'warning' as const : 'info' as const,
            source: 'Journal',
            symbol: entry.symbol
          }))
        ].slice(0, 8)
      },
      {
        id: 'data-quality',
        title: 'Data Quality',
        summary: staleAssets.length ? `${staleAssets.length} symbols require source/freshness review.` : 'Provider freshness is acceptable for current research workflows.',
        items: staleAssets.length
          ? staleAssets.map((asset) => ({
            label: `${asset.symbol} ${asset.metadata?.freshness ?? 'fallback'}`,
            detail: `${assetProvider(asset)} confidence ${asset.metadata?.confidence ?? 50}%. ${asset.metadata?.error ?? 'Review freshness before relying on analysis.'}`,
            severity: asset.metadata?.freshness === 'failed' ? 'critical' : 'warning',
            source: assetProvider(asset),
            symbol: asset.symbol,
            freshness: asset.metadata?.freshness
          }))
          : [item('Provider context usable', 'No stale, failed, or fallback warnings dominate the current compact market state.', 'positive', 'Provider pipeline')]
      }
    ],
    actionChecklist: [
      item('Review portfolio risk state', `Risk state is ${riskState}; check concentration before adding paper exposure.`, riskState === 'Healthy' ? 'positive' : 'warning', 'Risk engine'),
      topSignal
        ? item('Review top paper setup', `${topSignal.symbol}: confirmation is still manual; invalidation is ${topSignal.invalidation}`, 'info', 'Scanner engine', topSignal.symbol)
        : item('Refresh scanner context', 'No leading setup is currently available; refresh providers before planning.', 'warning', 'Scanner engine'),
      staleAssets.length
        ? item('Resolve data-quality warnings', `${staleAssets.length} symbols are stale, failed, fallback, or demo-labeled.`, 'warning', 'Provider pipeline')
        : item('Confirm provider freshness', 'Provider metadata is currently acceptable; keep checking source labels.', 'positive', 'Provider pipeline'),
      openPositions.length
        ? item('Check open paper positions', `${openPositions.length} paper holdings need stop, exposure, and journal review.`, 'info', 'Portfolio engine')
        : item('No open paper holdings', 'Use the scanner queue to create research plans before any paper execution.', 'info', 'Portfolio engine')
    ],
    disclosure: DISCLOSURE
  };
}

export function buildEndOfDayReview(state: AppState, inputs: ResearchWorkflowInputs): ResearchWorkflow {
  const filled = state.orders.filter((order) => order.status === 'filled');
  const rejected = state.orders.filter((order) => order.status === 'rejected');
  const topSignals = [...state.signals].sort((a, b) => b.score - a.score).slice(0, 5);
  const riskState = state.core.portfolio.riskState;
  const journalReminders = inputs.journal.filter((entry) => entry.entryType === 'mistake' || entry.lesson || entry.grade).slice(0, 5);
  const alertReview = inputs.alertHistory.slice(0, 8);
  const confidence = briefingConfidence(state);

  return {
    id: 'end-of-day-review',
    title: 'End-of-Day Review',
    generatedAt: state.generatedAt,
    confidence,
    summary: `${money(state.account.dailyPnl)} paper daily P/L, ${filled.length} filled paper orders, ${rejected.length} rejected paper orders, and ${riskState} risk state.`,
    sections: [
      {
        id: 'portfolio-review',
        title: 'Portfolio Review',
        summary: `${money(state.account.equity)} equity, ${money(state.account.exposure)} simulated exposure, ${money(state.account.unrealizedPnl)} unrealized P/L.`,
        items: [
          item('Daily paper P/L', `${money(state.account.dailyPnl)} (${pct(state.account.dailyPnlPct)})`, state.account.dailyPnl >= 0 ? 'positive' : 'warning', 'Portfolio engine'),
          item('Max drawdown', pct(state.account.maxDrawdownPct), state.account.maxDrawdownPct < -0.08 ? 'warning' : 'info', 'Risk engine'),
          ...state.core.portfolio.concentration.slice(0, 4).map((row) => item(`${row.symbol} concentration`, `${pct(row.weight)} of equity, ${money(row.value)} market value.`, row.weight > state.settings.maxPositionPct ? 'warning' : 'info', 'Risk engine', row.symbol))
        ]
      },
      {
        id: 'paper-orders',
        title: 'Paper Order Review',
        summary: `${filled.length} filled, ${rejected.length} rejected, ${state.orders.length} total recent paper orders.`,
        items: state.orders.slice(0, 8).map((order) => item(`${order.symbol} ${order.side}`, `${order.status} at ${money(order.price)} for ${money(order.notional)}. ${order.reason}`, order.status === 'rejected' ? 'warning' : 'info', 'Paper engine', order.symbol))
      },
      {
        id: 'scanner-performance',
        title: 'Scanner Performance Notes',
        summary: `${topSignals.length} top setups should be compared against next-session follow-through.`,
        items: topSignals.map((signal) => item(signal.symbol, `${signal.setupType}; ${watchNext(signal)} ${signal.invalidation}`, signal.riskScore >= 70 ? 'warning' : 'info', 'Scanner engine', signal.symbol))
      },
      {
        id: 'alerts',
        title: 'Alert Review',
        summary: `${alertReview.length} recent alert events are available for review.`,
        items: alertReview.length
          ? alertReview.map((alert) => item(alert.title, alert.detail, alert.severity === 'error' ? 'critical' : alert.severity === 'warning' ? 'warning' : 'info', 'Alert history', alert.symbol))
          : [item('No alert events', 'No recent alert history requires review.', 'positive', 'Alert history')]
      },
      {
        id: 'journal-reminders',
        title: 'Journal Reminders',
        summary: journalReminders.length ? 'Behavioral notes are available for review.' : 'No journal reminders are currently linked.',
        items: journalReminders.length
          ? journalReminders.map((entry) => item(entry.symbol ?? 'Journal', `${entry.mistake ? `Mistake: ${entry.mistake}. ` : ''}${entry.lesson ? `Lesson: ${entry.lesson}. ` : ''}${summarizeText(entry.note, 130)}`, entry.mistake ? 'warning' : 'info', 'Journal', entry.symbol))
          : [item('Add one closing note', 'Record one observation about risk, patience, or setup quality before the next session.', 'info', 'Journal')]
      }
    ],
    actionChecklist: [
      item('Archive today context', 'Generate or save the EOD review after paper orders, alerts, and journal notes are checked.', 'info', 'Reports'),
      item('Prepare next-day watchlist', topSignals.length ? `Start with ${topSignals.slice(0, 4).map((signal) => signal.symbol).join(', ')} and remove any stale-data names.` : 'Refresh scanner data before building tomorrow watchlist.', 'info', 'Scanner engine'),
      item('Review risk drift', `Risk state finished as ${riskState}; compare against morning state if available in report history.`, riskState === 'Healthy' ? 'positive' : 'warning', 'Risk engine')
    ],
    disclosure: DISCLOSURE
  };
}

export function buildSymbolTimeline(symbol: string, state: AppState, inputs: ResearchWorkflowInputs): ResearchTimelineEvent[] {
  const normalized = symbol.toUpperCase();
  const asset = state.market.find((item) => item.symbol === normalized);
  const signal = state.signals.find((item) => item.symbol === normalized);
  const events: ResearchTimelineEvent[] = [];

  if (asset) {
    events.push({
      id: `quote-${normalized}-${asset.updatedAt}`,
      timestamp: asset.updatedAt,
      type: 'quote',
      title: `${normalized} quote refreshed`,
      detail: `${money(asset.price)} current price, ${pct(asset.changePct)} change, ${asset.metadata?.confidence ?? 50}% data confidence from ${assetProvider(asset)}.`,
      severity: asset.metadata?.fallback || asset.metadata?.freshness === 'stale' ? 'warning' : 'info',
      source: assetProvider(asset),
      symbol: normalized,
      freshness: asset.metadata?.freshness
    });
  }

  if (signal) {
    events.push({
      id: `signal-${normalized}-${state.generatedAt}`,
      timestamp: state.generatedAt,
      type: 'signal',
      title: `${normalized} signal reviewed`,
      detail: buildSignalNarrative(signal, asset, state),
      severity: signal.riskScore >= 70 ? 'warning' : signal.score >= 75 ? 'positive' : 'info',
      source: 'Scanner engine',
      symbol: normalized,
      freshness: asset?.metadata?.freshness
    });
  }

  for (const alert of inputs.alertHistory.filter((entry) => entry.symbol === normalized).slice(0, 20)) {
    events.push({
      id: `alert-${alert.id}`,
      timestamp: alert.createdAt,
      type: 'alert',
      title: alert.title,
      detail: alert.detail,
      severity: alert.severity === 'error' ? 'critical' : alert.severity === 'warning' ? 'warning' : 'info',
      source: 'Alert history',
      symbol: normalized
    });
  }

  for (const entry of inputs.journal.filter((item) => item.symbol === normalized).slice(0, 20)) {
    events.push({
      id: `journal-${entry.id}`,
      timestamp: entry.createdAt,
      type: 'journal',
      title: `${entry.entryType.replace(/_/g, ' ')} note`,
      detail: `${entry.grade ? `Grade ${entry.grade}. ` : ''}${entry.mistake ? `Mistake: ${entry.mistake}. ` : ''}${entry.lesson ? `Lesson: ${entry.lesson}. ` : ''}${summarizeText(entry.note, 180)}`,
      severity: entry.mistake ? 'warning' : 'info',
      source: 'Journal',
      symbol: normalized
    });
  }

  for (const order of state.orders.filter((item) => item.symbol === normalized).slice(0, 20)) {
    events.push({
      id: `order-${order.id}`,
      timestamp: order.createdAt,
      type: 'paper-order',
      title: `${order.side.toUpperCase()} ${order.status}`,
      detail: `${order.qty} paper units at ${money(order.price)}; ${order.reason}`,
      severity: order.status === 'rejected' ? 'warning' : 'info',
      source: 'Paper engine',
      symbol: normalized
    });
  }

  for (const report of inputs.reports.filter((item) => JSON.stringify(item.payload).includes(normalized) || item.title.includes(normalized)).slice(0, 8)) {
    events.push({
      id: `report-${report.id}`,
      timestamp: report.createdAt,
      type: 'report',
      title: report.title,
      detail: report.summary,
      severity: report.freshness === 'failed' || report.freshness === 'fallback' ? 'warning' : 'info',
      source: report.source,
      symbol: normalized,
      freshness: report.freshness
    });
  }

  return events.sort((a, b) => Date.parse(b.timestamp) - Date.parse(a.timestamp)).slice(0, 60);
}

export function buildSignalNarrative(signal: Signal, asset: MarketAsset | undefined, state: AppState) {
  const strengths = componentStrengths(signal).slice(0, 2);
  const weaknesses = componentWeaknesses(signal).slice(0, 2);
  const dataConfidence = asset?.metadata?.confidence ?? state.core.providerHealth.confidence;
  const freshness = asset?.metadata?.freshness ?? state.core.providerHealth.freshness;
  const setup = signal.side === 'buy' ? 'paper long setup' : signal.side === 'sell' ? 'paper exit review' : 'research watch setup';
  const confidenceClause = dataConfidence < 55
    ? `Research confidence is reduced because provider confidence is ${dataConfidence}% and freshness is ${freshness}.`
    : `Research confidence is ${dataConfidence}% with ${freshness} source context.`;
  const weaknessClause = weaknesses.length
    ? `Constraints: ${weaknesses.join(', ')}.`
    : 'No single component is currently dominating the risk explanation.';
  return `${signal.symbol} is a ${setup}; momentum remains ${signal.momentumScore >= 75 ? 'constructive' : signal.momentumScore >= 55 ? 'mixed but improving' : 'unconfirmed'} with ${strengths.join(', ') || 'limited component strength'}. ${weaknessClause} ${confidenceClause} ${watchNext(signal)} Manual confirmation remains required.`;
}

export function watchNext(signal: Signal) {
  if (signal.side === 'sell') return 'Watch whether the paper exit condition remains valid after the next refresh.';
  if (signal.momentumScore >= 75) return `Watch for confirmation above ${money(signal.entry)} and respect invalidation near ${money(signal.stop)}.`;
  if (signal.components.volume < 55) return 'Watch for stronger participation before treating the setup as actionable.';
  if (signal.components.regime < 50) return 'Watch market regime context because it is reducing confidence.';
  return 'Watch for trend and participation to stay aligned across the next provider refresh.';
}

export function composeResearchReport(kind: SavedReport['kind'], state: AppState, inputs: ResearchWorkflowInputs, symbol?: string) {
  const workflow = kind === 'morning'
    ? buildMorningBriefing(state, inputs)
    : kind === 'eod'
      ? buildEndOfDayReview(state, inputs)
      : null;
  const provider = state.core.providerHealth;
  const selectedSignal = symbol ? state.signals.find((signal) => signal.symbol === symbol) : [...state.signals].sort((a, b) => b.score - a.score)[0];
  const selectedAsset = symbol ? state.market.find((asset) => asset.symbol === symbol) : undefined;
  const title = workflow?.title ?? reportTitle(kind, symbol ?? selectedAsset?.symbol);
  const summary = workflow?.summary ?? reportSummary(kind, state, selectedSignal?.symbol ?? symbol, provider.freshness);
  const html = workflow ? workflowHtml(workflow, provider) : generalReportHtml(title, summary, kind, state, selectedSignal, selectedAsset);
  return {
    kind,
    title,
    status: 'ready' as const,
    source: provider.coreAvailable ? 'AuralithCore' : 'Auralith web intelligence workflow',
    freshness: provider.freshness,
    summary,
    html,
    payload: {
      provider,
      account: state.account,
      portfolio: state.core.portfolio,
      workflow,
      symbol,
      selectedSignal,
      selectedAsset
    }
  };
}

function componentStrengths(signal: Signal) {
  return Object.entries(signal.components)
    .filter(([, value]) => value >= 60)
    .sort((a, b) => b[1] - a[1])
    .map(([key, value]) => `${componentLabel(key)} ${Math.round(value)}/100`);
}

function componentWeaknesses(signal: Signal) {
  return Object.entries(signal.components)
    .filter(([, value]) => value < 55)
    .sort((a, b) => a[1] - b[1])
    .map(([key, value]) => `${componentLabel(key)} ${Math.round(value)}/100`);
}

function reportTitle(kind: SavedReport['kind'], symbol?: string) {
  return {
    morning: 'Morning Briefing',
    eod: 'End-of-Day Review',
    weekly: 'Weekly Portfolio Review',
    risk: 'Risk Report',
    strategy: 'Strategy Report',
    symbol: `${symbol ?? 'Symbol'} Research Pack`
  }[kind];
}

function reportSummary(kind: SavedReport['kind'], state: AppState, symbol: string | undefined, freshness: DataFreshness) {
  if (kind === 'weekly') return `${state.positions.length} paper holdings, ${money(state.account.exposure)} simulated exposure, and ${money(state.account.cash)} cash are ready for weekly review.`;
  if (kind === 'risk') return `Portfolio risk is ${state.core.portfolio.riskState}; ${state.core.portfolio.warnings.length || 0} warnings require review before adding new paper risk.`;
  if (kind === 'strategy') return `${state.signals.length} scanner outputs are available for strategy review with paper-only manual confirmation still enforced.`;
  if (kind === 'symbol') return `${symbol ?? 'Selected symbol'} research pack combines quote metadata, scanner setup, journal context, alert coverage, and paper planning disclosure.`;
  return `${symbol ?? 'The scanner'} leads the current research queue; data freshness is ${freshness}.`;
}

function workflowHtml(workflow: ResearchWorkflow, provider: AppState['core']['providerHealth']) {
  const sections = workflow.sections.map((section) => `
    <section>
      <h2>${escapeHtml(section.title)}</h2>
      <p>${escapeHtml(section.summary)}</p>
      <table>${section.items.map((entry) => row(entry.label, entry.detail, entry.severity, entry.source)).join('')}</table>
    </section>
  `).join('');
  const checklist = workflow.actionChecklist.map((entry) => row(entry.label, entry.detail, entry.severity, entry.source)).join('');
  return reportShell(workflow.title, workflow.summary, `
    <div class="notice">${escapeHtml(workflow.disclosure)}</div>
    <table>
      ${row('Generated', new Date(workflow.generatedAt).toLocaleString(), 'info', 'Auralith')}
      ${row('Provider', provider.source, 'info', 'Provider pipeline')}
      ${row('Freshness', provider.freshness, provider.fallback ? 'warning' : 'info', 'Provider pipeline')}
      ${row('Research confidence', `${workflow.confidence}%`, workflow.confidence >= 70 ? 'positive' : workflow.confidence >= 50 ? 'warning' : 'critical', 'Auralith intelligence')}
      ${row('Paper mode', 'ON, manual confirmation required', 'positive', 'Safety gate')}
    </table>
    ${sections}
    <section>
      <h2>Action Checklist</h2>
      <table>${checklist}</table>
    </section>
  `);
}

function generalReportHtml(title: string, summary: string, kind: SavedReport['kind'], state: AppState, signal?: Signal, asset?: MarketAsset) {
  return reportShell(title, summary, `
    <div class="notice">${escapeHtml(DISCLOSURE)}</div>
    <table>
      ${row('Generated', new Date().toLocaleString(), 'info', 'Auralith')}
      ${row('Kind', kind, 'info', 'Reports')}
      ${row('Provider', state.core.providerHealth.source, state.core.providerHealth.fallback ? 'warning' : 'info', 'Provider pipeline')}
      ${row('Freshness', state.core.providerHealth.freshness, state.core.providerHealth.fallback ? 'warning' : 'info', 'Provider pipeline')}
      ${row('Risk state', state.core.portfolio.riskState, state.core.portfolio.riskState === 'Healthy' ? 'positive' : 'warning', 'Risk engine')}
      ${row('Top context', signal ? buildSignalNarrative(signal, asset, state) : 'No current scanner leader is available.', 'info', 'Scanner engine')}
    </table>
  `);
}

function reportShell(title: string, summary: string, body: string) {
  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>${escapeHtml(title)}</title>
  <style>
    body { font-family: Inter, Arial, sans-serif; background: #080a0f; color: #eef3f8; margin: 0; padding: 40px; }
    main { max-width: 960px; margin: 0 auto; }
    h1 { font-size: 34px; margin: 0 0 10px; }
    h2 { font-size: 18px; margin: 30px 0 8px; }
    p { color: #a8b0bc; line-height: 1.6; }
    table { width: 100%; border-collapse: collapse; margin-top: 16px; }
    td { border-top: 1px solid rgba(255,255,255,.08); padding: 12px 8px; vertical-align: top; }
    td:first-child { color: #8f9aaa; text-transform: uppercase; font-size: 12px; letter-spacing: .08em; width: 28%; }
    td:last-child small { color: #7f8a98; display: block; margin-top: 4px; }
    .notice { border: 1px solid rgba(109,168,255,.25); background: rgba(109,168,255,.08); padding: 14px 16px; border-radius: 10px; margin: 20px 0; }
    .positive { color: #9ccf9c; } .warning { color: #d9b96a; } .critical { color: #f0847f; } .info { color: #9db7d8; }
  </style>
</head>
<body>
  <main>
    <p>Auralith Research Workflow</p>
    <h1>${escapeHtml(title)}</h1>
    <p>${escapeHtml(summary)}</p>
    ${body}
  </main>
</body>
</html>`;
}

function row(label: string, detail: string, severity: WorkflowSeverity, source: string) {
  return `<tr><td>${escapeHtml(label)}</td><td><span class="${severity}">${escapeHtml(detail)}</span><small>${escapeHtml(source)}</small></td></tr>`;
}

function item(label: string, detail: string, severity: WorkflowSeverity, source: string, symbol?: string): ResearchWorkflowItem {
  return { label, detail, severity, source, symbol };
}

function briefingConfidence(state: AppState) {
  const provider = state.core.providerHealth.confidence;
  const stalePenalty = state.market.filter((asset) => asset.metadata?.fallback || asset.metadata?.freshness === 'stale' || asset.metadata?.freshness === 'failed').length ? 10 : 0;
  const signalBonus = state.signals.length ? 4 : -6;
  return Math.max(5, Math.min(95, Math.round(provider - stalePenalty + signalBonus)));
}

function summarizeText(value: string, limit: number) {
  return value.length <= limit ? value : `${value.slice(0, Math.max(0, limit - 3)).trim()}...`;
}

function componentLabel(value: string) {
  return value.replace(/([A-Z])/g, ' $1').replace(/^./, (letter) => letter.toUpperCase());
}

function assetProvider(asset: MarketAsset) {
  return asset.metadata?.provider ?? asset.source;
}

function assetClassLabel(asset: MarketAsset) {
  return asset.assetClass === 'crypto' ? 'Crypto' : 'Stock';
}

function money(value: number) {
  return new Intl.NumberFormat('en-US', { style: 'currency', currency: 'USD', maximumFractionDigits: Math.abs(value) > 1000 ? 0 : 2 }).format(value);
}

function pct(value: number) {
  return `${(value * 100).toFixed(2)}%`;
}

function escapeHtml(value: string) {
  return value
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}
