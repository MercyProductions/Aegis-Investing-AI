-- Aegis Stock Betting AI local store schema v1.
-- This is the migration target for replacing TSV/CSV persistence with SQLite.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_migrations (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL,
    description TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS holdings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    shares REAL NOT NULL CHECK (shares >= 0),
    average_cost REAL NOT NULL CHECK (average_cost >= 0),
    note TEXT NOT NULL DEFAULT '',
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_holdings_symbol ON holdings(symbol);

CREATE TABLE IF NOT EXISTS price_alerts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    trigger_price REAL NOT NULL CHECK (trigger_price >= 0),
    direction TEXT NOT NULL CHECK (direction IN ('above', 'below')),
    enabled INTEGER NOT NULL DEFAULT 1,
    note TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_price_alerts_symbol_enabled ON price_alerts(symbol, enabled);

CREATE TABLE IF NOT EXISTS alert_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    alert_id INTEGER,
    symbol TEXT NOT NULL,
    triggered_at TEXT NOT NULL,
    trigger_price REAL NOT NULL,
    observed_price REAL NOT NULL,
    acknowledged_at TEXT,
    snoozed_until TEXT,
    note TEXT NOT NULL DEFAULT '',
    FOREIGN KEY (alert_id) REFERENCES price_alerts(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_alert_events_symbol_time ON alert_events(symbol, triggered_at);

CREATE TABLE IF NOT EXISTS trade_plans (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    created_at TEXT NOT NULL,
    symbol TEXT NOT NULL,
    rating TEXT NOT NULL,
    entry REAL NOT NULL,
    stop REAL NOT NULL,
    target REAL NOT NULL,
    shares INTEGER NOT NULL,
    planned_risk REAL NOT NULL,
    planned_reward REAL NOT NULL,
    thesis TEXT NOT NULL DEFAULT '',
    status TEXT NOT NULL DEFAULT 'Open'
);

CREATE INDEX IF NOT EXISTS idx_trade_plans_symbol_status ON trade_plans(symbol, status);

CREATE TABLE IF NOT EXISTS symbol_notes (
    symbol TEXT PRIMARY KEY,
    tags TEXT NOT NULL DEFAULT '',
    note TEXT NOT NULL DEFAULT '',
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS trade_journal (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    time TEXT NOT NULL,
    symbol TEXT NOT NULL,
    action TEXT NOT NULL,
    reason TEXT NOT NULL DEFAULT '',
    exit_reason TEXT NOT NULL DEFAULT '',
    tags TEXT NOT NULL DEFAULT '',
    mistakes TEXT NOT NULL DEFAULT '',
    grade TEXT NOT NULL DEFAULT '',
    realized_pnl REAL NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_trade_journal_symbol_time ON trade_journal(symbol, time);

CREATE TABLE IF NOT EXISTS provider_cache (
    provider TEXT NOT NULL,
    endpoint TEXT NOT NULL,
    cache_key TEXT NOT NULL,
    fetched_at TEXT NOT NULL,
    expires_at TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT '',
    body TEXT NOT NULL,
    PRIMARY KEY (provider, endpoint, cache_key)
);

CREATE INDEX IF NOT EXISTS idx_provider_cache_expiry ON provider_cache(expires_at);

CREATE TABLE IF NOT EXISTS diagnostics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
    severity TEXT NOT NULL,
    provider TEXT NOT NULL,
    endpoint TEXT NOT NULL DEFAULT '',
    symbol TEXT NOT NULL DEFAULT '',
    status TEXT NOT NULL DEFAULT '',
    http_status INTEGER NOT NULL DEFAULT 0,
    duration_ms INTEGER NOT NULL DEFAULT 0,
    used_cache INTEGER NOT NULL DEFAULT 0,
    detail TEXT NOT NULL DEFAULT '',
    error TEXT NOT NULL DEFAULT ''
);

CREATE INDEX IF NOT EXISTS idx_diagnostics_provider_time ON diagnostics(provider, timestamp);
CREATE INDEX IF NOT EXISTS idx_diagnostics_symbol_time ON diagnostics(symbol, timestamp);

CREATE TABLE IF NOT EXISTS audit_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
    actor TEXT NOT NULL,
    action TEXT NOT NULL,
    symbol TEXT NOT NULL DEFAULT '',
    detail TEXT NOT NULL DEFAULT '',
    immutable_hash TEXT NOT NULL DEFAULT ''
);

CREATE INDEX IF NOT EXISTS idx_audit_log_action_time ON audit_log(action, timestamp);
CREATE INDEX IF NOT EXISTS idx_audit_log_symbol_time ON audit_log(symbol, timestamp);

INSERT OR IGNORE INTO schema_migrations(version, applied_at, description)
VALUES (1, datetime('now'), 'Initial Aegis stock research local store schema');
