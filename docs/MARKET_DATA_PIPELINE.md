# Auralith Market Data Pipeline

Auralith should be treated as a market intelligence aggregation system, not as a single-API dashboard. The durable architecture is:

```text
Providers
  -> normalization
  -> memory cache
  -> SQLite historical/cache store
  -> MarketEngine
  -> signals, risk, portfolio, charts, reports, AI
  -> desktop terminal and web cockpit
```

## Principles

- No anonymous market data reaches UI, scanners, reports, or AI summaries.
- Every market object carries provider, fetched time, freshness, fallback state, delayed/live state, and confidence.
- Provider failures are normal operating conditions, not exceptional UI states.
- Cache fallback is allowed, but it must be labeled.
- Demo data is allowed for offline workflows, but it must be labeled as fallback/demo data.
- UI code should consume normalized engine outputs, not provider-specific payloads.

## Normalized Quote Contract

Native `StockQuote` and web `MarketAsset.metadata` should preserve this contract:

| Field | Purpose |
| --- | --- |
| `provider` | The provider or adapter that produced the object. |
| `fetchedAt` / `fetched_at` | When the data was fetched, generated, or loaded from cache. |
| `freshness` | `live`, `delayed`, `stale`, `fallback`, or `failed` on web; `Live`, `Delayed`, `Stale`, `Fallback`, or `Error` in native provider metadata. |
| `delayed` | True when the provider or cache is not live enough for high-confidence intraday context. |
| `fallback` | True when demo/cache/provider fallback is being used. |
| `confidence` / `data_confidence` | Research confidence score reduced by fallback, stale data, provider errors, and estimated fundamentals. |
| `error` / `provider_error` | Provider notice, parse failure, rate limit, entitlement issue, or transport error. |

The contract is intentionally asset-class neutral so Auralith can later support stocks, ETFs, crypto, forex, commodities, and options without rewriting UI assumptions.

## Provider Tiers

### Current Foundation

| Provider | Role | Notes |
| --- | --- | --- |
| Alpha Vantage | Quotes, market status, daily candles, indicators, fundamentals/news/earnings slots | Primary native provider today. Some plans return delayed or limited payloads. |
| SEC EDGAR | Filings and company reports | Must keep a compliant user agent and throttling. |
| CoinGecko | Web crypto market board | Useful web companion feed; not a replacement for equity providers. |
| Yahoo Chart | Web stock quote companion feed | Supplemental and unofficial; must remain replaceable. |
| Stooq | Web stock fallback | Good fallback for delayed historical-style quotes. |
| Demo Provider | Offline layout and workflow data | Always labeled as fallback/demo. |

### Preferred Next Providers

| Provider | Best Use |
| --- | --- |
| Finnhub | Fundamentals, news, earnings, insider activity. |
| Financial Modeling Prep | Fundamentals and statements. |
| Twelve Data | Candles and multi-timeframe data. |
| FRED | Macro data: rates, CPI, unemployment, yield curve. |
| Polygon | Professional real-time/aggregate/options upgrade path. |
| Tiingo | High-quality historical data. |

## Routing Policy

The provider layer should route requests like this:

```text
request quote/candles/research bundle
  -> check memory cache
  -> check SQLite cache
  -> fetch preferred provider if cache is stale or force-live is enabled
  -> normalize provider response
  -> persist normalized result and diagnostics
  -> fallback to secondary provider or cache if needed
  -> fallback to demo only when no provider/cache is usable
```

Provider routing should be centralized in the provider/engine boundary. UI modules should not choose Alpha Vantage, Yahoo, Stooq, or demo directly.

## Freshness Policy

| State | Meaning |
| --- | --- |
| Live | Provider-backed data suitable for current monitoring. |
| Delayed | Provider-backed, but plan/venue/feed delay limits confidence. |
| Stale | Cache exists but exceeds freshness policy. |
| Fallback | Cache, demo, or secondary-provider fallback is active. |
| Error | Provider returned no usable normalized object. |

When freshness is not high, Auralith should reduce confidence and explain why:

```text
Research confidence: Medium. Price data is delayed and fundamentals are estimated.
```

## Cache Policy

Auralith needs three cache layers:

| Layer | Role |
| --- | --- |
| Memory cache | Prevents repeated provider calls during active UI sessions. |
| SQLite cache | Persists quotes, candles, indicators, provider health, scanner history, and research bundles. |
| Historical store | Owns candles, retention policies, missing-data repair, and backtest/replay inputs. |

The cache must store provider metadata with the payload. Cached data without provenance should be treated as low-confidence.

## Background Jobs

AuralithCore should eventually own these jobs:

- quote refresh
- watchlist refresh
- candle sync
- fundamentals/news/earnings sync
- SEC filings sync
- macro/FRED sync
- provider health checks
- stale-data validation
- cache cleanup
- backup snapshots
- report generation

Each job should expose status, progress, last run, last error, and retry/cancel where reasonable.

## Data Categories

Auralith should architect for these categories, but implement them in priority order:

| Category | Examples |
| --- | --- |
| Quotes | price, bid/ask, volume, market status |
| Historical candles | OHLCV for intraday, daily, weekly, monthly |
| Fundamentals | valuation, EPS, revenue, margins, debt, cash flow |
| Earnings/events | dates, estimates, surprises, revisions |
| News/sentiment | headlines, source, timestamp, sentiment |
| SEC filings | 10-K, 10-Q, 8-K, 13F, insider forms |
| ETFs/exposure | holdings, sector, overlap |
| Macro | CPI, Fed funds, jobs, yield curve |
| Options | IV, expected move, chain summaries |
| Breadth/sector | advancers, decliners, rotation, sector strength |
| Corporate actions | splits, dividends, symbol changes |

## Implementation Notes

- Native `StockQuote` now carries explicit provider metadata alongside legacy `source`, `timestamp`, and `data_quality`.
- Web `MarketAsset` now carries optional `metadata` with the same intent.
- `Store` persists market metadata columns so web refreshes do not collapse back to a bare source string.
- Scanner, risk, portfolio, and AI layers should use confidence/freshness when ranking or explaining signals.
- Reports should cite provider and timestamp for every quote bundle and research section.

## Non-Goals

- Do not hardwire Auralith to a single provider.
- Do not hide fallback/demo data.
- Do not treat unofficial feeds as a permanent professional data foundation.
- Do not imply live autonomous trading from provider access.
- Do not present low-confidence data with high-confidence language.
