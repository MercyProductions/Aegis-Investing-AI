# Web Parity Through AuralithCore

The website should mature into a premium universal-access client of AuralithCore, not a separate React rewrite of the desktop terminal.

Desktop remains the advanced workstation and workflow validation surface. The website should adopt proven desktop workflows through stable Core contracts.

## Contract First

The web parity contract is `auralith-core-web-v1`.

Initial endpoints:

- `GET /api/core`
- `GET /api/core/dashboard`
- `GET /api/core/quotes`
- `GET /api/core/watchlists`
- `GET /api/core/scanner`
- `GET /api/core/portfolio`
- `GET /api/core/alerts`
- `GET /api/core/reports`
- `GET /api/core/journal`
- `GET /api/core/symbol/:symbol`
- `GET /api/core/provider-health`
- `GET /api/core/paper-orders`
- `GET /api/core/ai-summaries`

These endpoints return Core-shaped data for:

- Quotes
- Watchlists
- Scanner results
- Portfolio
- Alerts
- Reports
- Journal
- Symbol research
- Provider health
- Paper orders
- AI summaries

## UI Direction

The web dashboard should prioritize:

- Market ribbon
- Portfolio snapshot
- Risk state
- Watchlist movers
- Scanner highlights
- AI briefing
- Alerts
- Provider freshness
- Paper-only status

The web app should remain calmer and simpler than the desktop terminal while still being serious enough for Mac, iPhone, Android, Linux, and browser-first users.

## Parity Rule

Do not duplicate business logic across platforms.

Keep centralized:

- Signal generation
- Momentum logic
- Risk analysis
- AI summaries
- Provider normalization
- Report generation
- Safety gates

## Current Status

- Web state now carries a `core` object with the stable contract.
- The web server can read `AuralithCore.exe --once` for native Core provider status when available.
- Dashboard provider status, AI briefing, scanner highlights, watchlists, portfolio snapshot, reports, journal, alerts, and symbol research now render from Core-shaped data.

## Next Work

1. Move scanner generation fully behind AuralithCore instead of the TypeScript strategy bridge.
2. Add alert CRUD endpoints backed by the Core alert engine.
3. Add journal persistence endpoints backed by Core storage.
4. Promote report generation from report cards to real Core-generated HTML/PDF outputs.
5. Expand symbol research with real fundamentals, earnings, news, SEC filing rows, and historical chart bundles from Core providers.

