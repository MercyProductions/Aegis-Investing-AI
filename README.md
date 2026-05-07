# Auralith

Native C++/Win32/DirectX11 Dear ImGui desktop client for the Auralith market intelligence and paper execution workflow.

This app is a research and paper-planning workstation. It is not financial advice, and live execution is intentionally not enabled by default.

Product direction: Auralith should mature as an AI-assisted portfolio research terminal for disciplined long-term investing, paper strategy development, historical research continuity, and local-first ownership. Keep the native desktop terminal as the advanced reference implementation while the web cockpit gradually adopts proven workflows through shared engines and services. AuralithCore is the shared intelligence layer being extracted in provider-first order. See `docs/PRODUCT_DOCTRINE.md`, `docs/PLATFORM_STRATEGY.md`, `docs/CORE_EXTRACTION_PLAN.md`, `docs/WEB_PARITY_THROUGH_CORE.md`, `docs/CROSS_PLATFORM_WEB_PLATFORM.md`, `docs/OPERATIONAL_EXCELLENCE.md`, `docs/LONGEVITY_CHARTER.md`, and `docs/STEWARDSHIP_MODE.md` before expanding major workflows.

## Repository Layout

- `src/` - C++ application, providers, analytics, diagnostics, storage facades, and self-tests.
- `imgui/` - Dear ImGui source and Win32/DX11 backends.
- `website/` - standalone Auralith Markets React/Express web cockpit used by the Open Web bridge.
- `Shared/` - shared model contracts used to align desktop, core, web, reports, SDK, and plugin boundaries.
- `docs/` - user guide, production hardening notes, SQLite schema, and project backlog.
- `scripts/` - repeatable build, test, and release scripts.
- `AegisStockBettingAI.config.ini` - safe default config template with no API key.

## Build

Open `AegisStockBettingAI.sln` in Visual Studio 2022 with the C++ desktop workload, or run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
```

The executable is written to `x64\Release\AegisStockBettingAI.exe`.

## Website Cockpit

The companion Auralith Markets website now lives inside this repository under `website/`. It should remain a focused command center for monitoring, reports, alerts, portfolio review, and lightweight research while the desktop app remains the primary advanced terminal. Runtime-only folders such as `node_modules/`, `dist/`, `logs/`, `data/`, and local `.env` files are intentionally ignored.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\launch-website.ps1
```

Then open:

```text
http://127.0.0.1:5176
```

The desktop app's Open Web button uses `website_base_url` plus `website_path` from `AegisStockBettingAI.config.ini`, which defaults to the local Vite cockpit at `http://127.0.0.1:5176/`. The older PHP auth bridge remains separate at `auth_base_url` for optional website login.

## Test

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test.ps1
```

The test script builds Release, runs `AegisStockBettingAI.exe --self-test`, starts the desktop app briefly, and verifies it stays alive for the smoke window.

## Market Data

The app runs in demo mode until an Alpha Vantage API key is saved in Settings. With a key, it refreshes `GLOBAL_QUOTE` for the configured watchlist and checks `MARKET_STATUS` for venue state. Alpha Vantage data freshness depends on the key entitlement and market-data plan.

The long-term data architecture is a multi-provider aggregation pipeline: providers normalize into a shared quote/research contract, cache through memory and SQLite, then feed the market engine, scanners, risk, portfolio, reports, AI, desktop terminal, and web cockpit. See `docs/MARKET_DATA_PIPELINE.md`.

The scanner now produces momentum-based paper setup plans instead of blind buy/sell calls. It scores trend, volume, RSI/MACD, breakout strength, sector context, and regime, then drafts entry, invalidation, stop, expected sell zones, trailing stop, risk/reward, and manual-confirmation requirements. See `docs/MOMENTUM_SETUP_ENGINE.md`.

API keys and remembered website credentials are encrypted with Windows DPAPI under the current Windows user profile.

## Portfolio

The Portfolio tab currently saves holdings to the legacy compatibility folder `%LOCALAPPDATA%\Aegis\Stock Betting AI\portfolio-holdings.tsv` and uses the active quote board to calculate market value, allocation, unrealized P/L, sector exposure, position concentration, and paper sizing. Holdings are automatically included in the refresh watchlist so saved positions stay marked even if they are not in the manual watchlist text box.

The same tab can save sizing worksheet outputs as paper trade plans in `%LOCALAPPDATA%\Aegis\Stock Betting AI\trade-plans.tsv`. Plans preserve entry, stop, target, shares, planned risk/reward, thesis, and an Open/Active/Closed status cycle. The trade plan table marks plans against the current quote board, shows plan P/L and target/stop progress, and can add a selected paper plan to saved holdings. Position sizing supports fixed dollar, fixed risk, volatility-adjusted, ATR stop, and conservative Kelly-lite modes.

The Research tab saves custom price alerts to `%LOCALAPPDATA%\Aegis\Stock Betting AI\price-alerts.tsv`. Alert symbols are also included in refreshes, and active above/below triggers are surfaced beside the model-generated alert stream.

## Advanced Research

The Chart Lab tab adds historical candles, volume bars, crosshair inspection, RSI, MACD, SMA/EMA, Bollinger Bands, ATR, drawdown, relative strength versus SPY/QQQ, explainable score breakdowns, risk critics, similar setup notes, fundamentals, SEC filing links, news/sentiment rows, earnings snapshots, ETF exposure, options-provider estimates, and a signal backtest summary. With an Alpha Vantage key, selected-symbol candles are loaded from `TIME_SERIES_DAILY` in the background and cached locally; fundamentals/news/earnings are loaded from `OVERVIEW`, `NEWS_SENTIMENT`, and `EARNINGS`. Recent SEC filings use the SEC ticker/CIK list and submissions API, then open direct archive document links. Without a key or during rate limits, the app uses cache or generated/provider-ready data so the tab stays usable.

The Journal tab adds today's focus, saved screen presets, watchlist groups, watchlist/conviction heatmaps, alert history, persistent symbol notes, upgraded trade journal entries, model audit rows, and CSV import/export workflows. Notes are stored in `symbol-notes.tsv`; upgraded journal rows are stored in `trade-journal-upgraded.tsv`.

The Integrations tab shows provider readiness for Alpha Vantage, SEC EDGAR, Alpaca paper account sync, OpenBB-style providers, FRED/macro, broker CSV import, ETF holdings, options chains, and local storage. It also includes a first-run setup wizard, API health check, paper execution guardrails, data freshness, portfolio beta/drawdown/cash-drag/concentration, sector caps, and an estimated correlation matrix.

## Local Data

Diagnostics, audit logs, backups, TSV stores, and CSV exports are still written to the legacy compatibility folder `%LOCALAPPDATA%\Aegis\Stock Betting AI` until the SQLite migration executor is implemented.

Daily candle cache files are written to `%LOCALAPPDATA%\Aegis\Stock Betting AI\history-cache`. Fundamentals, SEC filing rows, news, and earnings bundle caches are written to `%LOCALAPPDATA%\Aegis\Stock Betting AI\research-cache`. SEC ticker/CIK lookup cache is written to `%LOCALAPPDATA%\Aegis\Stock Betting AI\sec-cache`.

CSV export files include `holdings-export.csv`, `alerts-export.csv`, `trade-plans-export.csv`, `symbol-notes-export.csv`, and `trade-journal-export.csv`. CSV import looks for `watchlist-import.csv`, `holdings-import.csv`, `alerts-import.csv`, `trade-plans-import.csv`, `symbol-notes-import.csv`, and `trade-journal-import.csv` in the same folder.

All saved, edited, deleted, imported, exported, and promoted workflow actions are appended to `audit-log.jsonl` and compatible export logs. The app stays in research/paper mode by default; live execution is not enabled.
