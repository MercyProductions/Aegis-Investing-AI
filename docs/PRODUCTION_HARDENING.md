# Production Hardening Map

This app is now set up around a more durable core:

- Structured diagnostics: `diagnostics.jsonl` captures timestamp, severity, provider, endpoint, symbol, HTTP status, duration, cache state, detail, and error.
- Structured audit: `audit-log.jsonl` records important local workflow actions and keeps the old TSV export for compatibility.
- Provider facade: `ProviderLayer.*` is the new home for provider policy, cache controls, broker profiles, strategy builder logic, and readiness rows.
- Cache policy: quote/history/research TTL, max cache size, force-live refresh, prune, reset cache, and backup validation are visible in the UI.
- SEC compliance: the HTTP user agent is configurable and defaults to a descriptive research-only string with contact placeholder.
- Request safety: history and research loads carry request IDs so stale symbol-switch responses are ignored.
- Shutdown safety: app exit waits for outstanding provider futures before destroying the UI.
- Self-tests: `--self-test` covers JSON parsing, indicators, backtests, strategy rules, diagnostics redaction, and SEC user-agent configuration.
- Alert engine: custom alerts now persist trigger events, acknowledgement state, snooze state, observed price, and source.
- Portfolio analytics: correlation matrix and beta exposure use available quote return histories instead of sector-only estimates.
- Broker import normalization: brokerage CSV files are parsed through named profiles with preview rows, error rows, and valid-only holding import.
- HTML reports: Chart Lab can export source-linked ticker research reports, and Dashboard can export a daily briefing without writing API keys.
- Compare mode and session restore: selected tab/symbol, compare basket, chart days, and strategy rule are restored between launches.
- Release scripts: `scripts/build.ps1`, `scripts/test.ps1`, and `scripts/release.ps1` create repeatable builds and clean artifacts.
- SQLite migration target: `docs/sqlite-schema-v1.sql` defines the local store schema for replacing TSV files.

Remaining high-value follow-up work:

- Replace TSV readers/writers with an actual SQLite runtime and migration executor.
- Expand provider modules for FRED, ETF holdings, options chains, and Alpaca paper sync.
- Add a real background alert monitor with acknowledge/snooze/toast notifications.
- Add chart event overlays, compare mode, saved layouts, drawing tools, and full provider event markers.
- Add broker CSV import preview screens and tax-lot normalization.
- Add source-linked thesis generation with explicit citations per claim.
- Add crash recovery/session restore and encrypted backup archives.
