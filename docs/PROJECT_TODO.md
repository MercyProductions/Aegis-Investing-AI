# Auralith Project To-Do

Generated: April 27, 2026

This is the working backlog for turning Auralith from a powerful prototype into a durable, trustworthy stock research workstation. Use this as the master to-do list.

Before adding large new features, apply `docs/PRODUCT_DOCTRINE.md`, `docs/PLATFORM_STRATEGY.md`, `docs/CORE_EXTRACTION_PLAN.md`, `docs/WEB_PARITY_THROUGH_CORE.md`, `docs/CROSS_PLATFORM_WEB_PLATFORM.md`, `docs/OPERATIONAL_EXCELLENCE.md`, `docs/LONGEVITY_CHARTER.md`, and `docs/STEWARDSHIP_MODE.md`. Mature Auralith should prioritize refinement, stability, AI memory quality, research continuity, risk intelligence, and workflow clarity over breadth.

## Priority Legend

- P0: Absolutely needed before the app should be considered production-ready.
- P1: Important improvements that materially improve trust, usability, or maintainability.
- P2: Useful additions once the core foundation is strong.
- P3: Future expansion ideas.

## Backlog Discipline

- Prefer simplifying, merging, or removing overlapping surfaces before adding new panels.
- Promote work that improves trust: validation, source/freshness labeling, auditability, confidence scoring, stability, and performance.
- Promote work that improves Auralith's real differentiators: AI memory, research continuity, portfolio risk intelligence, historical context, scenario simulation, and local-first ownership.
- Defer work that mainly adds novelty, hype, or screen count.
- Do not add live-trading ambiguity. Paper mode and manual confirmation remain core safety boundaries.
- Keep the desktop terminal as the advanced reference implementation while the website matures through shared services, shared contracts, and proven workflows.
- Do not duplicate business logic across desktop and web just to reach premature browser parity.
- Extract shared intelligence into AuralithCore in this order: provider layer, scanner engine, alert engine, portfolio engine, report engine, AI systems, historical storage.
- When the app is feature-complete enough, prioritize curation, intelligence quality, trust, operational maturity, and longevity over expansion.
- At functional maturity, default to stewardship: maintain, simplify, optimize, and selectively improve existing workflows.

## Current Foundation Already Started

- [x] Native C++ / Win32 / DirectX11 / Dear ImGui desktop app.
- [x] Dashboard, Watchlist, AI Scanner, Portfolio, Research, Chart Lab, Compare, Journal, Integrations, and Settings tabs.
- [x] Alpha Vantage quote/history/research integration.
- [x] SEC filings integration foundation.
- [x] Structured diagnostics JSONL and audit logging foundation.
- [x] Configurable SEC user agent/contact field.
- [x] Basic provider policy/freshness/cache rows.
- [x] Market data pipeline doctrine captured in `docs/MARKET_DATA_PIPELINE.md`.
- [x] Desktop-primary and web-companion platform strategy captured in `docs/PLATFORM_STRATEGY.md`.
- [x] AuralithCore extraction order captured in `docs/CORE_EXTRACTION_PLAN.md`.
- [x] Initial AuralithCore provider snapshot boundary added in `src/core/CoreIntelligence.*`.
- [x] Initial web parity contract through AuralithCore captured in `docs/WEB_PARITY_THROUGH_CORE.md`.
- [x] Cross-platform web platform direction captured in `docs/CROSS_PLATFORM_WEB_PLATFORM.md`.
- [x] Operational excellence and stewardship checklist captured in `docs/OPERATIONAL_EXCELLENCE.md`.
- [x] Alert events with acknowledgement/snooze support.
- [x] Broker import preview/normalization foundation.
- [x] HTML symbol research report and daily briefing export.
- [x] Self-test command path with parser, indicators, strategy, report, alert, broker, and storage checks.
- [x] Release build/test/release PowerShell scripts.
- [x] Session persistence extracted into `AppSession`.
- [x] Persistent app-data facade started in `AppStorage`.
- [x] SQLite v1 schema document exists.
- [x] Data-quality rows, estimated-value labels, and confidence decay for demo/estimated quote data.
- [x] Expanded command palette actions for ticker jump/add, alert creation, reports, briefings, settings, alerts, and web bridge.
- [x] UI preference persistence for theme, compact mode, font scale, and high contrast.
- [x] In-app keyboard shortcut and command reference.
- [x] Data-status badge and breakdown for live, demo, cache, stale, rate-limited, and offline states.
- [x] Momentum setup engine drafts paper-only plans with entry, invalidation, stop, expected sell zones, trailing stops, risk/reward, confidence, and manual confirmation.
- [x] First tab extraction started with `src/ui/SettingsTab.*`.
- [x] Central safety gate scaffold added in `src/services/SafetyGate.*`.
- [x] Central background task row model added in `src/services/BackgroundTaskManager.*`.

## Absolutely Needed

### Architecture And Maintainability

- [ ] P0: Continue refactoring `src/Main.cpp`. It is still far too large and owns too much UI, state, workflow, async, and persistence behavior.
- [ ] P0: Continue extracting tab renderers into separate modules/classes: `DashboardTab`, `WatchlistTab`, `ScannerTab`, `PortfolioTab`, `ResearchTab`, `ChartLabTab`, `CompareTab`, `JournalTab`, `IntegrationsTab`; `SettingsTab` has started.
- [ ] P0: Create a real `AppState` object that owns shared UI state, selected symbol, filters, workflow buffers, and current provider results.
- [ ] P0: Create service boundaries for `PortfolioService`, `TradePlanService`, `AlertService`, `ResearchService`, `ProviderService`, and `ReportService`.
- [x] P0: Create initial `SettingsService` boundary for settings normalization, guardrail health, and secret clearing.
- [ ] P0: Remove direct persistence calls from UI code wherever possible. UI should call services, services should call storage.
- [ ] P0: Standardize naming and ownership: UI modules render, services mutate workflows, storage persists, providers fetch external data.
- [ ] P1: Add a lightweight event/action system for saved/removed/promoted/refresh-complete events so tabs do not need to call each other directly.
- [ ] P1: Add a `Result<T>` or consistent status object pattern for provider/storage/service operations.
- [ ] P1: Move all reusable ImGui widgets into a `UiComponents` module.
- [ ] P1: Move all chart rendering into a dedicated `ChartRenderer` module.
- [x] P1: Move shared CSV parsing/export helpers into a dedicated `Csv` module.
- [ ] P1: Move all formatting helpers into a dedicated `Format` module.

### Storage And Data Durability

- [ ] P0: Implement actual SQLite runtime storage, not just the schema document.
- [ ] P0: Add SQLite migrations with schema versioning.
- [ ] P0: Add transaction-safe save operations for holdings, alerts, trade plans, notes, journals, diagnostics, audit logs, provider cache, and session state.
- [ ] P0: Migrate existing TSV/CSV local data into SQLite on first run.
- [ ] P0: Keep CSV/TSV as import/export formats, not the primary source of truth.
- [ ] P0: Add indexes for symbol, timestamp, provider, endpoint, action, and alert state queries.
- [ ] P0: Make audit log append-only in SQLite.
- [ ] P0: Add query/export UI for audit log and diagnostics.
- [ ] P0: Add storage validation on startup: missing DB, corrupt DB, migration failure, and backup recommended states.
- [ ] P0: Add one-click backup and one-click restore with validation before restore.
- [ ] P1: Add optional encrypted backup archives.
- [ ] P1: Add cache prune by provider, age, symbol, and max-size policy.
- [ ] P1: Add reset demo data without deleting real user data.
- [ ] P1: Add rollback behavior if a restore or migration fails.
- [ ] P1: Add storage health indicator in Settings/Integrations.

### Provider Layer And Data Freshness

- [ ] P0: Finish centralizing provider logic behind one provider interface.
- [ ] P0: Move direct HTTP/provider work out of scattered modules and into provider clients.
- [x] P0: Add provider request objects with endpoint, symbol, cache policy, timeout, retry policy, and cancellation ID.
- [x] P0: Add first-class quote/provider response metadata for source, fetched time, freshness, fallback, delayed state, and confidence in native and web market objects.
- [ ] P0: Extend provider response metadata to every history, research, news, filings, report, and scanner object: cache age, HTTP status, fallback reason, and rate-limit reason.
- [ ] P0: Add cancellation/debouncing for symbol switching, chart loads, research loads, and global refreshes.
- [ ] P0: Add retry/backoff behavior for provider failures.
- [x] P0: Add provider rate-limit tracking for Alpha Vantage and SEC.
- [x] P0: Add SEC throttling that respects SEC-friendly access patterns.
- [ ] P0: Make provider cache policy configurable per provider and endpoint.
- [x] P0: Add force-live refresh that bypasses cache safely and reports rate-limit risk.
- [x] P1: Add provider capability registry so future providers can declare support for quotes, history, fundamentals, news, filings, macro, options, and broker data.
- [x] P1: Add provider diagnostics panel with recent request table.
- [ ] P1: Add provider failover ordering.
- [ ] P1: Add offline mode that clearly shows cache-only data.
- [ ] P1: Add stale-data warnings on every decision-impacting panel.

### Real Data Quality

- [ ] P0: Replace decision-impacting synthetic estimates wherever possible.
- [x] P0: Clearly label any remaining estimated values as estimated.
- [ ] P0: Use real provider data for market cap, beta, fundamentals, earnings, dividends, and sector where available.
- [ ] P0: Calculate portfolio beta and correlations from historical returns, not sector estimates.
- [ ] P0: Use adjusted historical candles for backtests and portfolio analytics.
- [ ] P0: Add splits/dividends awareness in backtesting and historical analytics.
- [ ] P0: Improve ETF exposure using real ETF holdings data.
- [ ] P0: Improve options expected move using real option chain data before showing it as actionable.
- [x] P1: Add quote-level confidence decay when quote data is demo, stale, fallback, or estimated.
- [ ] P1: Extend confidence decay to history, news, and filings staleness.
- [x] P1: Add initial estimated/provider-backed data-quality rows for selected symbols.
- [ ] P1: Add data-source comparison where multiple providers disagree.
- [ ] P1: Add “estimated / provider-backed / unavailable” badges to every metric.

### Testing And Verification

- [ ] P0: Add a real automated test project or test runner beyond the current self-test path.
- [ ] P0: Add tests for SQLite migrations.
- [x] P0: Add tests for storage load/save and backup validation failure cases.
- [ ] P0: Add tests for one-click restore failure cases once restore UX exists.
- [x] P0: Add tests for provider cache hit, cache miss, stale cache, live fetch, rate limit, retry, and fallback.
- [x] P0: Add tests for CSV import/export with quotes, commas, blank rows, malformed rows, and duplicate symbols.
- [x] P0: Add baseline CSV parser/exporter tests for quoted commas and escaped quotes.
- [x] P0: Add tests for broker import profiles: Robinhood, Fidelity, Schwab, Webull, IBKR, and generic.
- [x] P0: Add tests for indicators: SMA, EMA, RSI, MACD, Bollinger Bands, ATR, drawdown, volatility.
- [x] P0: Add tests for backtest no-lookahead behavior.
- [x] P0: Add tests for position sizing modes.
- [x] P0: Add tests for scoring/model explanation and paper-only momentum setup boundaries.
- [x] P0: Add tests for SEC filing parsing and risk-factor diffing.
- [x] P0: Add tests for alert triggering, duplicate suppression, snooze, acknowledge, and history persistence.
- [x] P0: Add tests for secret redaction in logs, diagnostics, reports, and backups.
- [ ] P1: Add UI smoke tests for launching, switching tabs, loading data, and closing cleanly.
- [ ] P1: Add snapshot-style tests for report export.
- [ ] P1: Add build-time check that release artifacts include required docs/config/schema.

### Async, Threading, And Shutdown Safety

- [x] P0: Ensure every future/background job is cancellable, awaited, or safely ignored before shutdown.
- [x] P0: Use request IDs consistently for refresh, validation, history, research, alerts, and future provider jobs.
- [x] P0: Prevent stale provider results from overwriting newer symbol selections.
- [x] P0: Add the first central task manager model for background work.
- [x] P0: Add graceful shutdown state with timeout and diagnostics if a provider hangs.
- [x] P1: Add progress indicators for long-running jobs.
- [x] P1: Add per-task status rows in Diagnostics.

### Alerts And Notifications

- [x] P0: Build a real background alert monitor.
- [x] P0: Evaluate alerts on a schedule independent of manual refresh.
- [ ] P0: Persist alert trigger history in SQLite.
- [x] P0: Support acknowledge, snooze, enable/disable, and delete with audit trails.
- [x] P0: Add notification center with unread alert count.
- [ ] P0: Add Windows toast notifications.
- [x] P0: Add alert freshness/source labels so users know whether alert checks used live or cached quotes.
- [x] P1: Add alert history outcome tracking: trigger price, current price, change since trigger.
- [ ] P1: Add alert types: price above/below, percent move, volume spike, score change, earnings soon, filing posted, news sentiment change.
- [ ] P1: Add quiet hours and notification preferences.

### Security And Trust

- [ ] P0: Keep research-only guardrails visible across scoring, trade planning, reports, and exports.
- [ ] P0: Keep paper mode default forever.
- [ ] P0: Block accidental live execution pathways.
- [ ] P0: If broker execution is added later, require explicit unlock, paper/live separation, confirmations, and audit trail.
- [ ] P0: Add clear all secrets for every stored credential/token.
- [ ] P0: Ensure API keys never appear in diagnostics, reports, backups, copied text, or crash logs.
- [ ] P0: Add secret-redaction tests for Alpha Vantage keys, cookies, auth headers, bearer tokens, passwords, and URL query keys.
- [ ] P1: Add optional backup encryption.
- [ ] P1: Add diagnostics export preview showing what will be included.
- [ ] P1: Add local privacy/help page explaining where data is stored.

### Installer, Release, And Operations

- [ ] P0: Add app icon resource.
- [ ] P0: Add version resource metadata.
- [ ] P0: Add proper release package layout.
- [ ] P0: Add installer script using Inno Setup, NSIS, WiX, or MSIX.
- [ ] P0: Add config/data migration notes per version.
- [ ] P0: Add release manifest with build date, version, commit/source state if available, included docs, and smoke result.
- [ ] P0: Add local CI script that runs build, self-tests, smoke, packaging, and artifact validation.
- [ ] P1: Add GitHub Actions/Azure DevOps workflow if this project moves into a Git repo.
- [ ] P1: Add crash dump/minidump option with secret scrubbing.
- [ ] P1: Add release channel config: dev, beta, stable.

### Input Validation

- [ ] P0: Validate ticker symbols everywhere.
- [x] P0: Add central `SymbolRules` module for ticker normalization and validation.
- [x] P0: Wire central ticker validation into watchlist, broker import, holdings, alerts, and settings health.
- [x] P0: Normalize Settings watchlist symbols through `SettingsService` before saving/refreshing.
- [ ] P0: Normalize aliases and special ticker formats consistently.
- [x] P0: Add import preview/error rows for every CSV workflow import path.
- [x] P0: Validate holdings: symbol, shares, average cost, note length, duplicate handling.
- [x] P0: Validate alerts: symbol, trigger price, enabled state, duplicate rules.
- [x] P0: Validate trade plans: entry, stop, target, shares, and risk/reward. Earnings-date validation still belongs with the real earnings calendar.
- [x] P0: Validate journal rows: date, symbol, action, grade, and P/L.
- [x] P0: Add `ImportValidation` service for CSV holdings, alerts, trade plans, notes, and journal row validation.
- [x] P0: Report imported and rejected CSV workflow row counts after import.
- [x] P0: Validate watchlist size and symbol count against provider/rate-limit constraints.
- [ ] P1: Add ticker autocomplete/search.
- [x] P1: Add user-facing error messages that say exactly which rows failed and why.

## Needs Improvements

### UI And UX

- [ ] P1: Reduce tab density, especially Portfolio, Research, Integrations, and Settings.
- [ ] P1: Add resizable split panes.
- [ ] P1: Add compact/expanded modes per tab.
- [x] P1: Add loading spinners or progress indicators for active provider jobs.
- [ ] P1: Add empty states for watchlist, holdings, alerts, notes, journal, compare basket, and provider results.
- [x] P1: Save theme, compact mode, font scale, high contrast, and layout preferences reliably.
- [ ] P1: Add keyboard focus handling and predictable tab order.
- [x] P1: Add high contrast polish beyond the config flag.
- [x] P1: Add command palette actions for ticker search, add alert, save plan, export report, switch tab, refresh, open settings.
- [x] P1: Add keyboard shortcuts with a small help reference.
- [x] P1: Add notification center.
- [x] P1: Add Today's Focus view.
- [x] P1: Add better data-status badge: demo, live, stale, cache-only, rate-limited, offline.
- [ ] P2: Add detachable panels if practical in ImGui.
- [ ] P2: Add customizable dashboard layout.
- [ ] P2: Add saved workspaces.

### Chart Lab

- [ ] P1: Add zoom and pan.
- [x] P1: Add crosshair with OHLC/volume/indicator readout.
- [x] P1: Add timeframe buttons: 1M, 3M, 6M, YTD, 1Y, 3Y, 5Y.
- [x] P1: Add daily/weekly/monthly candle aggregation.
- [x] P1: Add indicator toggles: SMA, EMA, RSI, MACD, Bollinger Bands, ATR, volume, drawdown, relative strength.
- [x] P1: Persist current Chart Lab window and indicator layout in the app session.
- [ ] P1: Add chart markers for earnings, news, SEC filings, alerts, trade plans, entries, exits, dividends, and splits.
- [ ] P1: Add saved chart layouts.
- [ ] P1: Add indicator presets.
- [ ] P2: Add drawing tools: trendline, horizontal level, rectangle, note marker.
- [ ] P2: Add compare overlay against SPY, QQQ, sector ETF, and selected symbols.
- [ ] P2: Add chart export image.

### Screener And Watchlists

- [ ] P1: Add saved screen presets.
- [ ] P1: Make screen presets editable, duplicable, and exportable.
- [ ] P1: Add filters for momentum, volume spike, gap up/down, 52-week high/low, volatility, sector, market cap, price range, dividend yield, P/E, RSI, and distance from moving averages.
- [ ] P1: Add watchlist groups with persistent storage.
- [ ] P1: Add drag/drop between watchlist groups.
- [ ] P1: Add per-group screen presets.
- [ ] P1: Add symbol notes visible from watchlist rows.
- [ ] P2: Add heatmap of watchlist movers.
- [ ] P2: Add conviction grid.
- [ ] P2: Add watchlist import from text, CSV, and broker positions.

### Portfolio And Risk

- [ ] P1: Calculate true portfolio beta from historical returns.
- [ ] P1: Calculate true portfolio volatility and drawdown from holdings.
- [ ] P1: Add correlation matrix from aligned adjusted returns.
- [ ] P1: Add sector caps and warnings.
- [ ] P1: Add max position risk checks.
- [ ] P1: Add concentration risk.
- [ ] P1: Add cash drag analysis.
- [ ] P1: Add volatility-adjusted position sizing.
- [ ] P1: Add ATR stop sizing.
- [ ] P1: Add conservative Kelly-lite sizing.
- [ ] P1: Add portfolio rebalance suggestions.
- [ ] P2: Add tax lot tracking.
- [ ] P2: Add dividend income projection.
- [ ] P2: Add portfolio optimizer with risk budget and sector constraints.
- [ ] P2: Add scenario analysis: market down 2 percent, sector down 5 percent, high-volatility day.

### Backtesting And Strategy Builder

- [ ] P1: Add a proper strategy builder UI.
- [ ] P1: Support rules like `RSI < 35 AND CLOSE > SMA50`.
- [ ] P1: Add OR, parentheses, crossover, volume, ATR, and relative strength operators.
- [ ] P1: Save strategy presets.
- [ ] P1: Show win rate, max drawdown, average return, profit factor, best/worst trade, exposure, and trade count.
- [ ] P1: Add fees and slippage controls.
- [ ] P1: Add position sizing modes in backtests.
- [ ] P1: Prevent lookahead bias.
- [ ] P1: Use adjusted candles.
- [ ] P1: Add benchmark comparison against buy-and-hold and SPY.
- [ ] P2: Add walk-forward testing.
- [ ] P2: Add out-of-sample split.
- [ ] P2: Add Monte Carlo resampling of trades.

### Fundamentals, SEC, News, And Earnings

- [ ] P1: Expand fundamentals panel with revenue, EPS, margins, debt, free cash flow, P/E, PEG, dividend yield, ROE, revenue growth, EPS growth, cash, debt maturity if available.
- [ ] P1: Add real earnings calendar.
- [ ] P1: Add previous surprise and post-earnings drift tracking.
- [ ] P1: Add estimated move from options data when available.
- [ ] P1: Add SEC filing timeline.
- [ ] P1: Add 10-K / 10-Q / 8-K / Form 4 tabs.
- [ ] P1: Add risk-factor change tracker.
- [ ] P1: Add insider Form 4 summary.
- [ ] P1: Add news feed with source, time, sentiment, and changed-since-last-refresh marker.
- [ ] P1: Add source-linked thesis generation.
- [ ] P2: Add filing timeline overlays on charts.
- [ ] P2: Add earnings/news/SEC markers on candles.
- [ ] P2: Add risk factor diff report export.

### Trade Planning And Journal

- [ ] P1: Add trade checklist before saving/promoting a plan.
- [ ] P1: Include trend, volume, catalyst, risk/reward, earnings date, market regime, invalidation, and position size in checklist.
- [ ] P1: Strengthen “promote signal to plan” flow.
- [ ] P1: Strengthen “promote plan to holding” flow.
- [ ] P1: Require acknowledgement of paper/research-only status before promotion.
- [ ] P1: Add invalidation checklist.
- [ ] P1: Add screenshots/attachments if feasible.
- [ ] P1: Add journal analytics: win/loss stats, average R, grade distribution, mistake tags, setup tags, holding period, realized/unrealized P/L.
- [ ] P1: Add mistake pattern dashboard.
- [ ] P1: Add journal CSV import preview and error rows.
- [ ] P2: Add calendar view of trades.
- [ ] P2: Add per-strategy journal stats.

### Reports And Briefings

- [ ] P1: Improve HTML research report templates.
- [ ] P1: Add PDF export.
- [ ] P1: Add source citations for every report claim.
- [ ] P1: Add provider freshness block to every report.
- [ ] P1: Add report export for compare baskets.
- [ ] P1: Add portfolio risk report.
- [ ] P1: Add backtest report.
- [ ] P1: Add daily briefing report sections: movers, alerts, earnings, filings, news, stale data, portfolio risk, watchlist focus.
- [ ] P2: Add scheduled morning/evening briefing generation.
- [ ] P2: Add report templates.

## Additional Suggestions

### Data Integrations

- [ ] P2: FRED macro data: rates, CPI, unemployment, yield curve, dollar proxy, credit spreads if available.
- [ ] P2: ETF holdings integration for SPY, QQQ, sector ETFs, and thematic ETFs.
- [ ] P2: Options chain integration: IV rank, expected move, skew, open interest, unusual volume.
- [ ] P2: Alpaca paper account sync: positions, cash, orders, fills, and paper execution only.
- [ ] P2: Broker CSV import templates for Robinhood, Fidelity, Schwab, Webull, IBKR, E-Trade, Merrill, Vanguard, and M1.
- [ ] P2: OpenBB-style provider/plugin registry.
- [ ] P2: Add Nasdaq/NYSE market calendar provider.
- [ ] P2: Add dividend calendar provider.
- [ ] P2: Add analyst ratings provider if available.
- [ ] P2: Add short interest provider if available.
- [ ] P2: Add institutional ownership provider if available.
- [ ] P2: Add alternative data only if source and limitations are clear.

### AI And Modeling

- [ ] P2: Explainable score breakdown with editable weights.
- [ ] P2: Model confidence decay when data gets stale.
- [ ] P2: Regime detection: bullish, choppy, risk-off, high volatility, earnings season.
- [ ] P2: Thesis generator with bull/base/bear cases.
- [ ] P2: Risk critic: what would invalidate this trade?
- [ ] P2: Similar setups based on past signals and indicators.
- [ ] P2: Model audit dashboard: outcome after 1 day, 1 week, 1 month, and 3 months.
- [ ] P2: Track score changes over time.
- [ ] P2: Let user edit scoring weights and compare model versions.
- [ ] P2: Keep all AI output source-linked and traceable.
- [ ] P3: Local model/plugin path for offline summaries.
- [ ] P3: Natural-language query over local portfolio/watchlist data.

### Advanced Portfolio Features

- [ ] P2: Rebalance planner.
- [ ] P2: Sector/industry exposure limits.
- [ ] P2: Factor exposure estimates: growth, value, momentum, quality, volatility.
- [ ] P2: Drawdown stress testing.
- [ ] P2: Risk contribution by holding.
- [ ] P2: Capital allocation planner.
- [ ] P2: Dividend reinvestment projections.
- [ ] P2: Tax-aware holding view.
- [ ] P3: Household/account-level portfolio grouping.

### UI Extensions

- [ ] P2: Sector heatmap.
- [ ] P2: Watchlist movers heatmap.
- [ ] P2: Conviction grid.
- [ ] P2: Compare mode with chart/score/fundamentals/news/filings side by side.
- [ ] P2: Saved layouts.
- [ ] P2: Full local help/docs tab.
- [ ] P2: First-run setup wizard improvements.
- [ ] P2: Better onboarding for API key, watchlist, portfolio cash, and paper-mode expectations.
- [ ] P2: Theme persistence and theme editor.
- [ ] P3: Multi-window support if ImGui/Win32 structure supports it cleanly.

### Reliability And Recovery

- [ ] P1: Crash recovery for unsaved notes, active plan edits, selected tab, selected symbol, filters, and compare basket.
- [ ] P1: Session restore should recover pending workflows safely.
- [ ] P1: Add autosave for notes/journal drafts.
- [ ] P1: Add corruption detection for local files and SQLite DB.
- [ ] P1: Add backup reminder when data changes significantly.
- [ ] P2: Add restore point before migrations.
- [ ] P2: Add diagnostics bundle with redacted config, provider status, build version, and recent logs.

## Recommended Build Order

### Phase 1: Clean Foundation

- [x] P0: Extract Settings tab from `Main.cpp`.
- [ ] P0: Extract Integrations tab from `Main.cpp`.
- [ ] P0: Extract Portfolio tab from `Main.cpp`.
- [ ] P0: Extract Journal tab from `Main.cpp`.
- [ ] P0: Extract Chart Lab tab from `Main.cpp`.
- [ ] P0: Create shared `AppState`.
- [ ] P0: Keep build/test green after each extraction.

### Phase 2: Real Storage

- [ ] P0: Add SQLite library/build integration.
- [ ] P0: Implement DB open/create/migrate.
- [ ] P0: Implement tables from `docs/sqlite-schema-v1.sql`.
- [ ] P0: Migrate holdings, alerts, alert events, trade plans, notes, journal, diagnostics, audit, session, and provider cache.
- [ ] P0: Add backup/restore UI.
- [x] P0: Add storage tests for the current TSV facade and backup validation.
- [ ] P0: Add SQLite storage tests after SQLite runtime integration.

### Phase 3: Provider Hardening

- [x] P0: Create provider request/response types.
- [ ] P0: Move Alpha Vantage calls behind provider client.
- [ ] P0: Move SEC calls behind provider client.
- [ ] P0: Add cancellation/debouncing.
- [ ] P0: Add retry/backoff/rate-limit state.
- [ ] P0: Show provider status and freshness everywhere.

### Phase 4: Decision Quality

- [ ] P0: Replace or label synthetic estimates.
- [ ] P0: Upgrade historical candles to adjusted data where possible.
- [ ] P0: Calculate true beta/correlation/drawdown/volatility from history.
- [ ] P0: Upgrade backtesting realism.
- [ ] P0: Add source-linked score explanation.

### Phase 5: Workflow Power

- [x] P1: Build background alert monitor.
- [ ] P1: Add Windows toast notifications.
- [ ] P1: Add true earnings calendar.
- [ ] P1: Add SEC risk-factor diff tracker.
- [ ] P1: Add strategy builder UI.
- [ ] P1: Add journal analytics.
- [ ] P1: Add better Chart Lab interactions.

### Phase 6: Packaging And Trust

- [ ] P0: Add installer.
- [ ] P0: Add version/app icon resources.
- [ ] P0: Add release validation.
- [ ] P0: Add encrypted backup option.
- [ ] P0: Add complete local help/privacy docs.

## Definition Of Done For Major Features

- [ ] Feature is separated from `Main.cpp` when practical.
- [ ] Feature has clear service/storage/provider ownership.
- [ ] Feature has tests or self-test coverage.
- [ ] Feature reports source, freshness, and fallback where data matters.
- [ ] Feature writes audit events for important user actions.
- [ ] Feature avoids leaking secrets in logs/reports/exports.
- [ ] Feature handles empty/error/loading states.
- [ ] Feature passes Release build, self-test, and smoke test.
