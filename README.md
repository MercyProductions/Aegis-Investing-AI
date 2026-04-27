# Aegis Stock Betting AI

Native C++/Win32/DirectX11 Dear ImGui desktop client for an Aegis stock investing research workflow.

This app is a research and paper-planning workstation. It is not financial advice, and live order execution is intentionally not enabled by default.

## Repository Layout

- `src/` - C++ application, providers, analytics, diagnostics, storage facades, and self-tests.
- `imgui/` - Dear ImGui source and Win32/DX11 backends.
- `docs/` - user guide, production hardening notes, SQLite schema, and project backlog.
- `scripts/` - repeatable build, test, and release scripts.
- `AegisStockBettingAI.config.ini` - safe default config template with no API key.

## Build

Open `AegisStockBettingAI.sln` in Visual Studio 2022 with the C++ desktop workload, or run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
```

The executable is written to `x64\Release\AegisStockBettingAI.exe`.

## Test

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test.ps1
```

The test script builds Release, runs `AegisStockBettingAI.exe --self-test`, starts the desktop app briefly, and verifies it stays alive for the smoke window.

## Market Data

The app runs in demo mode until an Alpha Vantage API key is saved in Settings. With a key, it refreshes `GLOBAL_QUOTE` for the configured watchlist and checks `MARKET_STATUS` for venue state. Alpha Vantage data freshness depends on the key entitlement and market-data plan.

API keys and remembered website credentials are encrypted with Windows DPAPI under the current Windows user profile.

## Portfolio

The Portfolio tab saves holdings to `%LOCALAPPDATA%\Aegis\Stock Betting AI\portfolio-holdings.tsv` and uses the active quote board to calculate market value, allocation, unrealized P/L, sector exposure, position concentration, and paper sizing. Holdings are automatically included in the refresh watchlist so saved positions stay marked even if they are not in the manual watchlist text box.

The same tab can save sizing worksheet outputs as paper trade plans in `%LOCALAPPDATA%\Aegis\Stock Betting AI\trade-plans.tsv`. Plans preserve entry, stop, target, shares, planned risk/reward, thesis, and an Open/Active/Closed status cycle. The trade plan table marks plans against the current quote board, shows plan P/L and target/stop progress, and can add a selected paper plan to saved holdings. Position sizing supports fixed dollar, fixed risk, volatility-adjusted, ATR stop, and conservative Kelly-lite modes.

The Research tab saves custom price alerts to `%LOCALAPPDATA%\Aegis\Stock Betting AI\price-alerts.tsv`. Alert symbols are also included in refreshes, and active above/below triggers are surfaced beside the model-generated alert stream.

## Advanced Research

The Chart Lab tab adds historical candles, volume bars, crosshair inspection, RSI, MACD, SMA/EMA, Bollinger Bands, ATR, drawdown, relative strength versus SPY/QQQ, explainable score breakdowns, risk critics, similar setup notes, fundamentals, SEC filing links, news/sentiment rows, earnings snapshots, ETF exposure, options-provider estimates, and a signal backtest summary. With an Alpha Vantage key, selected-symbol candles are loaded from `TIME_SERIES_DAILY` in the background and cached locally; fundamentals/news/earnings are loaded from `OVERVIEW`, `NEWS_SENTIMENT`, and `EARNINGS`. Recent SEC filings use the SEC ticker/CIK list and submissions API, then open direct archive document links. Without a key or during rate limits, the app uses cache or generated/provider-ready data so the tab stays usable.

The Journal tab adds today's focus, saved screen presets, watchlist groups, watchlist/conviction heatmaps, alert history, persistent symbol notes, upgraded trade journal entries, model audit rows, and CSV import/export workflows. Notes are stored in `symbol-notes.tsv`; upgraded journal rows are stored in `trade-journal-upgraded.tsv`.

The Integrations tab shows provider readiness for Alpha Vantage, SEC EDGAR, Alpaca paper trading, OpenBB-style providers, FRED/macro, broker CSV import, ETF holdings, options chains, and local storage. It also includes a first-run setup wizard, API health check, paper-trading guardrails, data freshness, portfolio beta/drawdown/cash-drag/concentration, sector caps, and an estimated correlation matrix.

## Local Data

Diagnostics, audit logs, backups, TSV stores, and CSV exports are written to `%LOCALAPPDATA%\Aegis\Stock Betting AI`.

Daily candle cache files are written to `%LOCALAPPDATA%\Aegis\Stock Betting AI\history-cache`. Fundamentals, SEC filing rows, news, and earnings bundle caches are written to `%LOCALAPPDATA%\Aegis\Stock Betting AI\research-cache`. SEC ticker/CIK lookup cache is written to `%LOCALAPPDATA%\Aegis\Stock Betting AI\sec-cache`.

CSV export files include `holdings-export.csv`, `alerts-export.csv`, `trade-plans-export.csv`, `symbol-notes-export.csv`, and `trade-journal-export.csv`. CSV import looks for `watchlist-import.csv`, `holdings-import.csv`, `alerts-import.csv`, `trade-plans-import.csv`, `symbol-notes-import.csv`, and `trade-journal-import.csv` in the same folder.

All saved, edited, deleted, imported, exported, and promoted workflow actions are appended to `audit-log.tsv`. The app stays in research/paper mode by default; live order execution is not enabled.
