# Auralith SQLite Migration Plan

This plan compares the current native TSV/JSONL persistence files with `docs/sqlite-schema-v1.sql`. It is a migration map and safety plan only; the native app still reads and writes the legacy files until the SQLite runtime layer is implemented.

## Current Legacy Folder

The native desktop app intentionally still uses the existing folder:

```text
%LOCALAPPDATA%\Aegis\Stock Betting AI
```

Keeping this path for the first Auralith pass avoids silently hiding user data. The future SQLite migration can add an Auralith-branded folder or alias after a verified backup/import path exists.

## Migration Map

| Legacy file | Current format | SQLite target | Mapping notes |
| --- | --- | --- | --- |
| `portfolio-holdings.tsv` | TSV rows | `holdings` | `symbol`, `shares`, `average_cost`, `note`; set `updated_at` to import time when missing. |
| `price-alerts.tsv` | TSV rows | `price_alerts` | `above=true` becomes `direction='above'`; `above=false` becomes `direction='below'`; set `created_at` and `updated_at` to import time when missing. |
| `alert-events.tsv` | TSV rows | `alert_events` | Preserve trigger time, symbol, trigger price, observed price, acknowledged/snoozed state, and note. Link `alert_id` when an imported alert rule matches. |
| `trade-plans.tsv` | TSV rows | `trade_plans` | Preserve created time, symbol, rating, entry, stop, target, shares, planned risk/reward, thesis, and status. |
| `symbol-notes.tsv` | TSV rows | `symbol_notes` | Use symbol as primary key; merge duplicate symbols by newest row or last imported row and record duplicates in diagnostics. |
| `trade-journal-upgraded.tsv` | TSV rows | `trade_journal` | Preserve time, symbol, action, reason, exit reason, tags, mistakes, grade, and realized P/L. |
| `app-session.tsv` | Key/value TSV | `app_session` | Store each key/value pair with import timestamp. This closes the schema gap for selected tab, selected symbol, compare basket, chart layout, and strategy rule. |
| `diagnostics.jsonl` | JSONL events | `diagnostics` | Map timestamp, severity, provider, endpoint, symbol, status, HTTP status, duration, cache state, detail, and error. Invalid JSONL rows should be copied to backup and reported, not discarded silently. |
| `audit-log.jsonl` | JSONL events | `audit_log` | Map timestamp, actor, action, symbol, detail, and immutable hash when present. Import should remain append-only. |

## First Run Behavior

1. Detect the legacy folder and any known TSV/JSONL files.
2. Create a timestamped backup directory before opening or importing data.
3. Copy every legacy file into the backup directory exactly as found.
4. Open/create the SQLite database and apply `schema_migrations`.
5. Import each file inside a transaction per logical dataset.
6. Leave all original legacy files untouched.
7. Write a migration summary to `audit_log` and `diagnostics`.
8. If any dataset fails, roll back that dataset transaction, keep the app on the legacy file backend, and show the failure in Integrations/Settings.

## Compatibility Layer

- Until the SQLite backend is proven, the app should keep a storage facade that can read from SQLite when migration succeeds and fall back to TSV/JSONL when it fails.
- Existing CSV import/export remains a user-facing interchange format, not the primary store.
- The backup routine must run before every migration attempt, even repeated attempts.
- Migration status should be visible in the Background Tasks table as `SQLite migration`.

## Validation Before Implementation

- Add self-tests for each file parser against empty files, malformed rows, duplicate symbols, missing optional fields, and invalid numeric values.
- Add a migration dry-run that counts rows without writing.
- Add a restore test proving the backup directory can repopulate the legacy files.
- Add an audit test proving blocked/failed migration attempts are recorded.
