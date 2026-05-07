# Auralith Operational Excellence

This phase is about stewardship. Do not aggressively add major systems. Improve speed, stability, trust, clarity, accessibility, and research quality in the workflows Auralith already owns.

## Current Refinement Pass

- Narrow `/api/core/dashboard` so it returns a dashboard payload instead of the entire app state.
- Return compact web state from `/api/state` by default, while preserving `/api/state?full=1` for diagnostics and deeper inspection.
- Bound the default web scanner universe so routine dashboard refreshes do not score every provider asset.
- Limit prebuilt symbol research to prioritized symbols: positions, recent paper orders, top scanner outputs, core indices, stale/fallback risk watch, and largest movers.
- Keep full symbol research available through `/api/core/symbol/:symbol`.
- Improve AI summary language with concrete evidence: provider state, fallback counts, exposure, largest position, risk/reward, and invalidation context.
- Improve report summaries so they read as concise research outputs, not generic placeholders.
- Add SQLite indexes for common web/Core daily-use queries.
- Add keyboard focus visibility and reduced-motion behavior to the web cockpit.

## Performance Priorities

- Avoid returning huge payloads to route-level clients when a compact contract is enough.
- Prefer endpoint-specific contracts for dashboard, provider health, symbol research, alerts, reports, and portfolio.
- Keep scanner and symbol research generation bounded.
- Add indexes before adding new query-heavy workflows.
- Measure payload size and route latency before broadening browser features.

## Long-Session Stability Checklist

- Leave the website and API running for at least 8 hours.
- Confirm provider refreshes continue without overlapping runaway jobs.
- Confirm `/api/health` stays responsive.
- Confirm stale or failed provider refreshes write audit events and recover on the next successful refresh.
- Confirm the web app restores the last saved session after the API is temporarily unavailable.
- Watch process memory during repeated dashboard, scanner, symbol, reports, and alerts navigation.
- Confirm service worker updates do not trap stale assets.

## Trust Signals

Every decision-impacting view should preserve:
- Provider/source.
- Fetched time or last refresh.
- Freshness state.
- Fallback/demo state.
- Confidence score.
- Paper-only status.
- Manual-confirmation requirement.

If a metric cannot explain its source or freshness, it should not be presented as decision-grade.

## AI Explanation Standard

Prefer explanations that include:
- What changed.
- Which evidence moved the score.
- What risk increased.
- What would invalidate the setup.
- How data quality affects confidence.
- Whether portfolio exposure changes the interpretation.

Avoid:
- Guaranteed language.
- Buy/sell commands.
- Prediction certainty.
- Generic summaries that merely restate a title.

## Release QA Checklist

- Native Release build passes.
- Native self-tests pass.
- Website typecheck passes.
- Website build passes.
- Website launcher reaches `http://127.0.0.1:5176`.
- `/api/health` returns paper mode with live trading locked.
- `/api/core/dashboard` returns a compact dashboard contract.
- `/api/core/symbol/AAPL` returns focused symbol research.
- Migration tests pass before any schema change ships.
- Stale-provider simulation produces warnings, not crashes.
- Report export test passes.
- Long-session smoke run is recorded.
- Changelog and release notes are updated.

## Stewardship Rule

Only add features when they improve research quality, reduce friction, improve continuity, improve risk visibility, or strengthen trust. Otherwise, refine what already exists.
