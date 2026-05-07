# Auralith Intelligence Network

The Intelligence Network phase turns Auralith into a persistent, local-first investment research operating environment.

## Core Service

`AuralithCore.exe` is the long-running local service foundation. It owns:

- Provider synchronization
- Background refresh
- Historical storage
- Alert monitoring
- Report readiness
- Task scheduling
- Workspace persistence
- Audit logging
- AI memory
- Notification dispatch planning
- Local API/IPC bridge planning

The desktop app remains the terminal/client. Core owns persistence and background intelligence.

## SQLite Persistence

The core database lives at:

`%LOCALAPPDATA%\Aegis\Stock Betting AI\data\auralith-core.sqlite`

The first schema creates:

- symbols
- quotes
- candles
- indicators
- alerts
- portfolios
- positions
- orders
- journal
- reports
- risk_events
- provider_health
- workspace_layouts
- ai_memory
- audit_events
- scanner_history
- backtest_runs

WAL mode, schema migrations, indexed queries, and backup/repair planning are part of the persistence boundary.

Each `AuralithCore --once` or daemon cycle now persists a local intelligence snapshot:

- symbols
- quotes
- provider health
- scanner history
- AI memory rows
- risk events
- audit event fingerprint

## AI Memory

The AI memory layer is intentionally cautious. It remembers:

- Prior notes
- Journal patterns
- Repeated mistakes
- Prior scanner outcomes
- Portfolio concentration patterns
- Failed/successful paper setups
- Historical volatility behavior

It should say things like:

- "This allocation increases concentration exposure."
- "Volatility remains elevated."
- "This resembles a prior failed paper breakout."

It must not say:

- "Buy this."
- "Sell this."
- "This will go up."

## Research Timeline

Every symbol should accumulate a timeline of alerts, earnings, news, filings, scanner signals, volatility spikes, journal entries, reports, paper trades, and strategy interactions.

## Local First

Profiles, secrets, workspaces, journals, strategies, and reports remain local by default. Optional cloud sync can be planned later, but local ownership is the primary architecture.

## Companion Planning

The mobile/web companion should focus on alerts, watchlists, reports, risk notifications, portfolio overview, paper order monitoring, and market-open briefing. The full research terminal remains desktop-first.

## Operational Maturity

Core tracks provider reliability, scanner performance, strategy performance, alert usefulness, data freshness, task timing, and future process metrics.
