# Auralith Markets

Auralith Markets is a private, local-first market research cockpit for crypto and stocks. The current build is intentionally paper-only: it can ingest public market quotes, scan a broad crypto/stock universe, score signals, simulate paper executions, enforce risk limits, keep an audit trail, and run as a server-backed PWA across desktop and mobile browsers.

It is not financial advice, and it does not place live trades in this version.

## Why Paper-Only First

Autonomous execution software can lose money quickly when a model, feed, broker, or strategy is wrong. Auralith therefore starts with:

- paper execution simulation only
- live execution locked in the backend
- explicit risk limits
- audit logs for every paper execution attempt and settings change
- public no-key market data first, with simulation fallback if providers fail
- manual-confirmation controls

## Run

```powershell
.\launch.ps1
```

Then open:

```text
http://127.0.0.1:5176
```

## Manual Commands

```powershell
npm install
npm run dev
npm run build
npm start
```

The production server listens on:

```text
http://127.0.0.1:4280
```

## Architecture

```text
React PWA cockpit
  -> Express API server
    -> public market data providers
    -> rotating stock/crypto scanner
    -> simulation fallback
    -> signal engine
    -> paper execution simulator
    -> risk engine
    -> audit trail
    -> SQLite storage
```

## Current Capabilities

- broad crypto scanner using CoinGecko market pages
- broad stock scanner using NASDAQ symbol directories plus quote refresh batches
- on-demand ticker add for stocks that are not on the board yet
- real quote ingestion from CoinGecko and Yahoo Chart, with Stooq as a stock fallback
- simulated ticks only when public quote refreshes fail or real mode is disabled
- portfolio/account summary
- signal scoring
- paper buy/sell simulations
- risk controls
- signal review toggle for paper mode
- audit trail
- mobile-friendly PWA UI
- local SQLite persistence

## Market Data Settings

The scanner is configured through `.env`:

```text
AURALITH_MARKET_DATA_MODE=real
AURALITH_MARKET_REFRESH_MS=90000
AURALITH_CRYPTO_PAGES=4
AURALITH_STOCK_UNIVERSE_MODE=listed
AURALITH_STOCK_SCAN_BATCH=180
AURALITH_STOCK_QUOTE_CONCURRENCY=20
```

Legacy `AEGIS_*` environment variable names are still accepted as fallbacks so existing local launch scripts keep working.

This means the app pulls up to 1,000 CoinGecko crypto markets and refreshes the core stock list plus a rotating batch from the larger NASDAQ-listed universe. The database keeps scanned assets, so coverage grows as the server runs.

Public no-key feeds can be delayed, rate-limited, revised, or temporarily unavailable. They are good enough for a private paper execution MVP and scanner prototype, but not enough for real-money autonomous execution. Live execution should use broker-grade market data and broker order adapters.

## Future Live Trading Connectors

Live connectors should be added only after paper execution simulation, risk controls, and backtests are reliable.

Possible adapters:

- Stocks: Alpaca paper/live API, Interactive Brokers Gateway
- Crypto: CCXT-supported exchange APIs
- Market data: broker feeds, Polygon, Alpaca data, exchange feeds

Every live adapter should require:

- explicit live unlock
- broker-specific risk limits
- kill switch
- max daily loss enforcement
- order preview mode
- secrets stored outside model-visible context
- audit logging
- paper/live parity tests

## Regulatory Notes

Day trading rules, broker restrictions, taxes, and margin requirements change over time and differ by account type and jurisdiction. Auralith should never assume a user is eligible to day trade. Broker-side restrictions must always be treated as the source of truth.

## Project Layout

```text
src/server/     Express API, market data, risk, paper broker
src/client/     React PWA cockpit
public/         PWA manifest and service worker
data/           Local SQLite runtime data
```
