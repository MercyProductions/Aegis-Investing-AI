# Aegis Stock Betting AI User Guide

## Dashboard

Use Dashboard for the daily watchlist, quote freshness, source label, selected-symbol detail, refresh controls, and the HTML daily briefing export. The source badge tells you whether data is live, cached, or demo fallback.

## Watchlist

Add uppercase ticker symbols, remove the selected symbol, and filter the quote board by movers, ETFs, technology, or semiconductors. Symbols are normalized before saving.

## AI Scanner

Scanner rows rank setups by explainable factors. Treat scores as research triage, not advice. Use the score breakdown and risk critic before promoting anything to a plan.

## Portfolio

Holdings, alerts, and trade plans live here. Position sizing supports fixed dollar, fixed risk, volatility-adjusted, ATR stop, and conservative Kelly-style sizing. Paper mode and manual confirmation should stay enabled.

## Research

Research shows signal factors, alerts, portfolio rules, and saved alert workflows. Custom alerts now create persistent trigger events that can be acknowledged or snoozed from Journal.

## Chart Lab

Chart Lab loads daily candles, indicators, fundamentals, SEC filings, news, earnings, options estimates, ETF exposure, checklists, model explanations, and backtests. Use Export HTML Report to create a source-linked ticker report in the app data `reports` folder. The Strategy Builder accepts expressions like:

```text
RSI < 35 AND CLOSE > SMA50
```

Supported rule inputs include `CLOSE`, `OPEN`, `HIGH`, `LOW`, `RSI`, `RSI14`, `SMA20`, `SMA50`, `EMA12`, `EMA26`, `MACD`, `ATR`, and `ATR14`.

## Compare

Compare mode shows a saved basket of up to six symbols side by side with price, movement, signal, score, confidence, target, stop, risk label, mini trend, quick technical/fundamental snapshot, and return correlation. Use Save Session to remember the compare basket.

## Journal

Journal tracks notes, focus items, watchlist groups, presets, alert history, heatmaps, trade notes, mistakes, grades, and realized win/loss stats. The alert engine records trigger time, observed price, source, note, acknowledgement state, and snooze state.

Broker Import Normalizer accepts `robinhood-import.csv`, `fidelity-import.csv`, `schwab-import.csv`, `webull-import.csv`, `ibkr-import.csv`, or `broker-import.csv` in the app data folder. It previews valid and error rows before importing holdings.

## Integrations

Integrations shows provider status, provider controls, cache policy, macro placeholders, risk analytics, data freshness, fallback reasons, diagnostics paths, backup/export controls, cache pruning, reset demo cache, and backup validation. Correlations use available quote return history instead of sector labels.

## Settings

Settings stores Alpha Vantage key, watchlist, SEC user-agent/contact, cache TTLs, force-live refresh, theme preferences, high contrast, font scaling, website bridge credentials, and security controls. Use Clear All Secrets before sharing diagnostics or artifacts.

## Local Files

App data is stored under:

```text
%LOCALAPPDATA%\Aegis\Stock Betting AI
```

Important files include `diagnostics.jsonl`, `audit-log.jsonl`, `portfolio-holdings.tsv`, `price-alerts.tsv`, `alert-events.tsv`, `trade-plans.tsv`, `symbol-notes.tsv`, `trade-journal-upgraded.tsv`, and optional broker import CSV files.

Session restore is stored in `app-session.tsv` and remembers selected tab, selected symbol, compare basket, chart days, and strategy rule.

HTML reports are written under:

```text
%LOCALAPPDATA%\Aegis\Stock Betting AI\reports
```

## Testing

Run:

```powershell
.\scripts\test.ps1
```

or:

```powershell
.\x64\Release\AegisStockBettingAI.exe --self-test
```
