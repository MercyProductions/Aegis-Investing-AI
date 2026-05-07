import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { DatabaseSync } from 'node:sqlite';
import type {
  AlertHistoryRecord,
  AlertRecord,
  AlertSeverity,
  AlertType,
  AuthResult,
  AuthSession,
  AuthUser,
  CandleRecord,
  DataFreshness,
  JournalRecord,
  MarketAsset,
  NotificationRecord,
  OrderRecord,
  Position,
  RiskSettings,
  SavedReport,
  StrategyRecord,
  WatchlistRecord,
  WorkspaceLayoutRecord
} from './types.js';

const DEFAULT_DB = 'data/auralith.sqlite';
const LEGACY_DB = 'data/aegis-trader.sqlite';

function defaultRiskSettings(): RiskSettings {
  return {
    mode: 'paper',
    liveTradingLocked: true,
    manualConfirmationRequired: true,
    autopilot: false,
    maxPositionPct: 0.12,
    maxTradeNotional: 10000,
    maxOpenPositions: 8,
    dailyLossLimitPct: 0.03,
    minConfidence: 0.72,
    disabledStrategies: []
  };
}

export class Store {
  readonly db: DatabaseSync;
  readonly dbPath: string;
  private readonly migratedLegacyDb: string | null;

  constructor(dbPath = process.env.AURALITH_TRADER_DB || process.env.AEGIS_TRADER_DB || DEFAULT_DB) {
    const migration = resolveDatabasePath(dbPath);
    const resolved = migration.resolved;
    this.dbPath = resolved;
    this.migratedLegacyDb = migration.migratedLegacyDb;
    fs.mkdirSync(path.dirname(resolved), { recursive: true });
    this.db = new DatabaseSync(resolved);
    this.db.exec('PRAGMA journal_mode = WAL;');
    this.db.exec('PRAGMA foreign_keys = ON;');
    this.init();
    if (this.migratedLegacyDb) {
      this.audit('info', 'storage.db_migrated', 'Copied legacy Aegis web database into Auralith persistence.', {
        from: this.migratedLegacyDb,
        to: resolved
      });
    }
  }

  init() {
    this.db.exec(`
      create table if not exists settings (
        key text primary key,
        value text not null
      );

      create table if not exists market_assets (
        symbol text primary key,
        name text not null,
        asset_class text not null,
        price real not null,
        previous_close real not null,
        momentum real not null,
        volatility real not null,
        volume real not null,
        source text not null default 'simulated',
        market_cap real,
        fetched_at text,
        freshness text not null default 'fallback',
        delayed integer not null default 1,
        fallback integer not null default 1,
        data_confidence integer not null default 50,
        provider_error text,
        updated_at text not null
      );

      create table if not exists account (
        id text primary key,
        cash real not null,
        starting_equity real not null,
        equity_high real not null,
        realized_pnl real not null,
        day_start_equity real not null,
        updated_at text not null
      );

      create table if not exists positions (
        symbol text primary key,
        asset_class text not null,
        qty real not null,
        avg_price real not null,
        last_price real not null,
        entry_time integer not null
      );

      create table if not exists orders (
        id text primary key,
        created_at text not null,
        symbol text not null,
        side text not null,
        qty real not null,
        status text not null,
        mode text not null,
        price real not null,
        notional real not null,
        reason text not null,
        signal text not null
      );

      create table if not exists audit_events (
        id integer primary key autoincrement,
        created_at text not null,
        level text not null,
        event_type text not null,
        message text not null,
        payload_json text not null
      );

      create table if not exists historical_prices (
        symbol text not null,
        timestamp integer not null,
        price real not null,
        volume real not null,
        primary key (symbol, timestamp)
      );

      create table if not exists strategy_performance (
        strategy text primary key,
        total_trades integer not null default 0,
        winning_trades integer not null default 0,
        total_pnl real not null default 0,
        avg_pnl real not null default 0,
        win_rate real not null default 0,
        last_updated text not null
      );

      create table if not exists auth_users (
        id text primary key,
        email text not null unique,
        workspace_name text not null,
        password_hash text not null,
        created_at text not null,
        updated_at text not null,
        last_login_at text
      );

      create table if not exists auth_sessions (
        id text primary key,
        user_id text not null,
        token_hash text not null unique,
        device_label text not null,
        created_at text not null,
        expires_at text not null,
        revoked_at text,
        foreign key (user_id) references auth_users(id) on delete cascade
      );

      create table if not exists password_reset_tokens (
        id text primary key,
        user_id text not null,
        token_hash text not null unique,
        created_at text not null,
        expires_at text not null,
        used_at text,
        foreign key (user_id) references auth_users(id) on delete cascade
      );

      create table if not exists email_verification_tokens (
        id text primary key,
        user_id text not null,
        token_hash text not null unique,
        created_at text not null,
        expires_at text not null,
        verified_at text,
        foreign key (user_id) references auth_users(id) on delete cascade
      );

      create table if not exists two_factor_secrets (
        user_id text primary key,
        secret_encrypted text not null,
        enabled integer not null default 0,
        created_at text not null,
        updated_at text not null,
        foreign key (user_id) references auth_users(id) on delete cascade
      );

      create table if not exists alerts (
        id text primary key,
        type text not null,
        symbol text,
        threshold real,
        percent_move real,
        severity text not null,
        enabled integer not null default 1,
        acknowledged_at text,
        snoozed_until text,
        last_triggered_at text,
        note text not null default '',
        created_at text not null,
        updated_at text not null
      );

      create table if not exists alert_history (
        id text primary key,
        alert_id text,
        created_at text not null,
        severity text not null,
        symbol text,
        title text not null,
        detail text not null,
        acknowledged_at text,
        foreign key (alert_id) references alerts(id) on delete set null
      );

      create table if not exists journal_entries (
        id text primary key,
        symbol text,
        entry_type text not null,
        tags_json text not null,
        note text not null,
        mistake text,
        lesson text,
        grade text,
        realized_pnl real,
        created_at text not null,
        updated_at text not null
      );

      create table if not exists saved_reports (
        id text primary key,
        kind text not null,
        title text not null,
        status text not null,
        source text not null,
        freshness text not null,
        summary text not null,
        html text not null,
        payload_json text not null,
        created_at text not null,
        updated_at text not null
      );

      create table if not exists candles (
        symbol text not null,
        asset_type text not null,
        timeframe text not null,
        timestamp integer not null,
        open real not null,
        high real not null,
        low real not null,
        close real not null,
        volume real not null,
        provider text not null,
        fetched_at text not null,
        freshness text not null,
        fallback integer not null default 0,
        synthetic integer not null default 0,
        primary key (symbol, timeframe, timestamp)
      );

      create table if not exists watchlists (
        id text primary key,
        name text not null,
        role text not null,
        tags_json text not null,
        symbols_json text not null,
        created_at text not null,
        updated_at text not null
      );

      create table if not exists saved_strategies (
        id text primary key,
        name text not null,
        description text not null,
        rules_json text not null,
        preset text not null,
        enabled integer not null default 0,
        created_at text not null,
        updated_at text not null
      );

      create table if not exists workspace_layouts (
        id text primary key,
        name text not null,
        mode text not null,
        config_json text not null,
        created_at text not null,
        updated_at text not null
      );

      create table if not exists notifications (
        id text primary key,
        severity text not null,
        source text not null,
        title text not null,
        detail text not null,
        related_symbol text,
        read_at text,
        snoozed_until text,
        created_at text not null
      );

      create index if not exists idx_market_assets_freshness on market_assets(freshness, updated_at);
      create index if not exists idx_market_assets_source on market_assets(source, updated_at);
      create index if not exists idx_orders_created_at on orders(created_at desc);
      create index if not exists idx_orders_symbol_created_at on orders(symbol, created_at desc);
      create index if not exists idx_audit_events_created_at on audit_events(created_at desc);
      create index if not exists idx_audit_events_type_created_at on audit_events(event_type, created_at desc);
      create index if not exists idx_strategy_performance_total_pnl on strategy_performance(total_pnl desc);
      create index if not exists idx_auth_users_email on auth_users(email);
      create index if not exists idx_auth_sessions_user_created on auth_sessions(user_id, created_at desc);
      create index if not exists idx_auth_sessions_token on auth_sessions(token_hash);
      create index if not exists idx_alerts_symbol_enabled on alerts(symbol, enabled, updated_at desc);
      create index if not exists idx_alert_history_created on alert_history(created_at desc);
      create index if not exists idx_journal_symbol_created on journal_entries(symbol, created_at desc);
      create index if not exists idx_journal_created on journal_entries(created_at desc);
      create index if not exists idx_saved_reports_kind_created on saved_reports(kind, created_at desc);
      create index if not exists idx_candles_symbol_timeframe_timestamp on candles(symbol, timeframe, timestamp);
      create index if not exists idx_watchlists_updated on watchlists(updated_at desc);
      create index if not exists idx_saved_strategies_updated on saved_strategies(updated_at desc);
      create index if not exists idx_workspace_layouts_mode_updated on workspace_layouts(mode, updated_at desc);
      create index if not exists idx_notifications_created on notifications(created_at desc);
      create index if not exists idx_notifications_read_created on notifications(read_at, created_at desc);
    `);

    this.migrateMarketColumns();

    if (!this.db.prepare('select id from account where id = ?').get('paper')) {
      this.db.prepare(
        'insert into account (id, cash, starting_equity, equity_high, realized_pnl, day_start_equity, updated_at) values (?, ?, ?, ?, ?, ?, ?)'
      ).run('paper', 100000, 100000, 100000, 0, 100000, now());
    }

    const defaults = defaultRiskSettings();
    for (const [key, value] of Object.entries(defaults)) {
      if (!this.db.prepare('select key from settings where key = ?').get(key)) {
        this.db.prepare('insert into settings (key, value) values (?, ?)').run(key, JSON.stringify(value));
      }
    }
    this.seedWebWorkflows();
  }

  settings(): RiskSettings {
    const rows = this.db.prepare('select key, value from settings').all() as Array<{ key: string; value: string }>;
    const data = Object.fromEntries(rows.map((row) => [row.key, JSON.parse(row.value)])) as Partial<RiskSettings>;
    return {
      ...defaultRiskSettings(),
      ...data,
      mode: 'paper',
      liveTradingLocked: true,
      manualConfirmationRequired: true
    };
  }

  updateSettings(next: Partial<RiskSettings>) {
    const current = this.settings();
    const merged = {
      ...current,
      ...next,
      liveTradingLocked: true,
      manualConfirmationRequired: true,
      mode: next.mode === 'live' ? 'paper' : next.mode ?? current.mode
    };
    for (const [key, value] of Object.entries(merged)) {
      this.db.prepare('insert into settings (key, value) values (?, ?) on conflict(key) do update set value = excluded.value').run(
        key,
        JSON.stringify(value)
      );
    }
    this.audit('info', 'settings.updated', 'Risk settings updated.', merged);
  }

  market(): MarketAsset[] {
    const rows = this.db.prepare('select * from market_assets order by asset_class, symbol').all() as any[];
    return rows.map((row) => ({
      symbol: row.symbol,
      name: row.name,
      assetClass: row.asset_class,
      price: row.price,
      previousClose: row.previous_close,
      changePct: pct(row.price, row.previous_close),
      momentum: row.momentum,
      volatility: row.volatility,
      volume: row.volume,
      source: row.source ?? 'simulated',
      marketCap: row.market_cap ?? undefined,
      updatedAt: row.updated_at,
      metadata: {
        provider: row.source ?? 'simulated',
        fetchedAt: row.fetched_at ?? row.updated_at,
        freshness: row.freshness ?? 'fallback',
        delayed: Boolean(row.delayed),
        fallback: Boolean(row.fallback),
        confidence: row.data_confidence ?? 50,
        error: row.provider_error ?? undefined
      }
    }));
  }

  upsertAsset(asset: Omit<MarketAsset, 'changePct'>) {
    const metadata = asset.metadata ?? {
      provider: asset.source,
      fetchedAt: asset.updatedAt,
      freshness: asset.source === 'simulated' ? 'fallback' : 'delayed',
      delayed: asset.source !== 'simulated',
      fallback: asset.source === 'simulated',
      confidence: asset.source === 'simulated' ? 55 : 70
    };
    this.db.prepare(`
      insert into market_assets (symbol, name, asset_class, price, previous_close, momentum, volatility, volume, source, market_cap, fetched_at, freshness, delayed, fallback, data_confidence, provider_error, updated_at)
      values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
      on conflict(symbol) do update set
        name = excluded.name,
        asset_class = excluded.asset_class,
        price = excluded.price,
        previous_close = excluded.previous_close,
        momentum = excluded.momentum,
        volatility = excluded.volatility,
        volume = excluded.volume,
        source = excluded.source,
        market_cap = excluded.market_cap,
        fetched_at = excluded.fetched_at,
        freshness = excluded.freshness,
        delayed = excluded.delayed,
        fallback = excluded.fallback,
        data_confidence = excluded.data_confidence,
        provider_error = excluded.provider_error,
        updated_at = excluded.updated_at
    `).run(
      asset.symbol,
      asset.name,
      asset.assetClass,
      asset.price,
      asset.previousClose,
      asset.momentum,
      asset.volatility,
      asset.volume,
      metadata.provider || asset.source,
      asset.marketCap ?? null,
      metadata.fetchedAt || asset.updatedAt,
      metadata.freshness,
      metadata.delayed ? 1 : 0,
      metadata.fallback ? 1 : 0,
      metadata.confidence,
      metadata.error ?? null,
      asset.updatedAt
    );
  }

  positions(): Position[] {
    const rows = this.db.prepare('select * from positions order by symbol').all() as any[];
    return rows.map((row) => {
      const marketValue = row.qty * row.last_price;
      const cost = row.qty * row.avg_price;
      const unrealizedPnl = marketValue - cost;
      return {
        symbol: row.symbol,
        assetClass: row.asset_class,
        qty: row.qty,
        avgPrice: row.avg_price,
        lastPrice: row.last_price,
        marketValue,
        unrealizedPnl,
        unrealizedPnlPct: cost ? unrealizedPnl / cost : 0,
        entryTime: row.entry_time
      };
    });
  }

  account() {
    return this.db.prepare('select * from account where id = ?').get('paper') as any;
  }

  orders(limit = 30): OrderRecord[] {
    const rows = this.db.prepare('select * from orders order by created_at desc limit ?').all(limit) as any[];
    return rows.map((row) => ({
      id: row.id,
      createdAt: row.created_at,
      symbol: row.symbol,
      side: row.side,
      qty: row.qty,
      status: row.status,
      mode: row.mode,
      price: row.price,
      notional: row.notional,
      reason: row.reason,
      signal: row.signal
    }));
  }

  audit(level: 'info' | 'warning' | 'error', eventType: string, message: string, payload: unknown = {}) {
    this.db.prepare('insert into audit_events (created_at, level, event_type, message, payload_json) values (?, ?, ?, ?, ?)').run(
      now(),
      level,
      eventType,
      message,
      JSON.stringify(payload)
    );
  }

  auditEvents(limit = 25) {
    return this.db.prepare('select id, created_at as createdAt, level, event_type as eventType, message from audit_events order by id desc limit ?').all(
      limit
    ) as any[];
  }

  alerts(limit = 200): AlertRecord[] {
    const rows = this.db.prepare('select * from alerts order by enabled desc, updated_at desc limit ?').all(limit) as any[];
    return rows.map((row) => this.alertFromRow(row));
  }

  createAlert(input: {
    type: AlertType;
    symbol?: string;
    threshold?: number;
    percentMove?: number;
    severity?: AlertSeverity;
    note?: string;
    enabled?: boolean;
  }) {
    const timestamp = now();
    const alert: AlertRecord = {
      id: crypto.randomUUID(),
      type: input.type,
      symbol: cleanSymbol(input.symbol),
      threshold: finiteOrNull(input.threshold) ?? undefined,
      percentMove: finiteOrNull(input.percentMove) ?? undefined,
      severity: input.severity ?? defaultSeverity(input.type),
      enabled: input.enabled ?? true,
      note: input.note?.trim() ?? '',
      createdAt: timestamp,
      updatedAt: timestamp
    };
    this.db.prepare(`
      insert into alerts (id, type, symbol, threshold, percent_move, severity, enabled, note, created_at, updated_at)
      values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `).run(alert.id, alert.type, alert.symbol ?? null, alert.threshold ?? null, alert.percentMove ?? null, alert.severity, alert.enabled ? 1 : 0, alert.note, alert.createdAt, alert.updatedAt);
    this.recordAlertHistory({
      alertId: alert.id,
      severity: alert.severity,
      symbol: alert.symbol,
      title: 'Alert Created',
      detail: alertDetail(alert)
    });
    this.audit('info', 'alert.created', `Alert created${alert.symbol ? ` for ${alert.symbol}` : ''}.`, alert);
    return alert;
  }

  updateAlert(id: string, input: Partial<Pick<AlertRecord, 'type' | 'symbol' | 'threshold' | 'percentMove' | 'severity' | 'enabled' | 'note'>>) {
    const current = this.alertById(id);
    if (!current) throw new Error('Alert not found.');
    const next: AlertRecord = {
      ...current,
      ...input,
      symbol: input.symbol !== undefined ? cleanSymbol(input.symbol) : current.symbol,
      threshold: input.threshold !== undefined ? finiteOrNull(input.threshold) ?? undefined : current.threshold,
      percentMove: input.percentMove !== undefined ? finiteOrNull(input.percentMove) ?? undefined : current.percentMove,
      note: input.note !== undefined ? input.note.trim() : current.note,
      updatedAt: now()
    };
    this.db.prepare(`
      update alerts set type = ?, symbol = ?, threshold = ?, percent_move = ?, severity = ?, enabled = ?, note = ?, updated_at = ?
      where id = ?
    `).run(next.type, next.symbol ?? null, next.threshold ?? null, next.percentMove ?? null, next.severity, next.enabled ? 1 : 0, next.note, next.updatedAt, id);
    this.audit('info', 'alert.updated', `Alert updated${next.symbol ? ` for ${next.symbol}` : ''}.`, next);
    return next;
  }

  deleteAlert(id: string) {
    const current = this.alertById(id);
    if (!current) return false;
    this.db.prepare('delete from alerts where id = ?').run(id);
    this.audit('info', 'alert.deleted', `Alert deleted${current.symbol ? ` for ${current.symbol}` : ''}.`, current);
    return true;
  }

  acknowledgeAlert(id: string) {
    const timestamp = now();
    this.db.prepare('update alerts set acknowledged_at = ?, updated_at = ? where id = ?').run(timestamp, timestamp, id);
    this.db.prepare('update alert_history set acknowledged_at = ? where alert_id = ? and acknowledged_at is null').run(timestamp, id);
    this.audit('info', 'alert.acknowledged', 'Alert acknowledged.', { id });
    return this.alertById(id);
  }

  snoozeAlert(id: string, minutes: number) {
    const until = new Date(Date.now() + Math.max(1, minutes) * 60 * 1000).toISOString();
    this.db.prepare('update alerts set snoozed_until = ?, updated_at = ? where id = ?').run(until, now(), id);
    this.audit('info', 'alert.snoozed', 'Alert snoozed.', { id, until });
    return this.alertById(id);
  }

  alertHistory(limit = 200): AlertHistoryRecord[] {
    const rows = this.db.prepare('select * from alert_history order by created_at desc limit ?').all(limit) as any[];
    return rows.map((row) => this.alertHistoryFromRow(row));
  }

  recordAlertHistory(input: {
    alertId?: string;
    severity: AlertSeverity;
    symbol?: string;
    title: string;
    detail: string;
  }) {
    const event: AlertHistoryRecord = {
      id: crypto.randomUUID(),
      alertId: input.alertId,
      createdAt: now(),
      severity: input.severity,
      symbol: cleanSymbol(input.symbol),
      title: input.title.trim(),
      detail: input.detail.trim()
    };
    this.db.prepare(`
      insert into alert_history (id, alert_id, created_at, severity, symbol, title, detail)
      values (?, ?, ?, ?, ?, ?, ?)
    `).run(event.id, event.alertId ?? null, event.createdAt, event.severity, event.symbol ?? null, event.title, event.detail);
    return event;
  }

  journalEntries(limit = 200, filters: { symbol?: string; tag?: string } = {}): JournalRecord[] {
    let rows: any[];
    if (filters.symbol) {
      rows = this.db.prepare('select * from journal_entries where symbol = ? order by created_at desc limit ?').all(cleanSymbol(filters.symbol) ?? '', limit) as any[];
    } else {
      rows = this.db.prepare('select * from journal_entries order by created_at desc limit ?').all(limit) as any[];
    }
    const entries = rows.map((row) => this.journalFromRow(row));
    if (!filters.tag) return entries;
    return entries.filter((entry) => entry.tags.includes(filters.tag!));
  }

  createJournalEntry(input: {
    symbol?: string;
    entryType?: JournalRecord['entryType'];
    tags?: string[];
    note: string;
    mistake?: string;
    lesson?: string;
    grade?: string;
    realizedPnl?: number;
  }) {
    const timestamp = now();
    const entry: JournalRecord = {
      id: crypto.randomUUID(),
      symbol: cleanSymbol(input.symbol),
      entryType: input.entryType ?? 'note',
      tags: cleanTags(input.tags),
      note: input.note.trim(),
      mistake: emptyToUndefined(input.mistake),
      lesson: emptyToUndefined(input.lesson),
      grade: emptyToUndefined(input.grade),
      realizedPnl: finiteOrNull(input.realizedPnl) ?? undefined,
      createdAt: timestamp,
      updatedAt: timestamp
    };
    this.db.prepare(`
      insert into journal_entries (id, symbol, entry_type, tags_json, note, mistake, lesson, grade, realized_pnl, created_at, updated_at)
      values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `).run(entry.id, entry.symbol ?? null, entry.entryType, JSON.stringify(entry.tags), entry.note, entry.mistake ?? null, entry.lesson ?? null, entry.grade ?? null, entry.realizedPnl ?? null, entry.createdAt, entry.updatedAt);
    this.audit('info', 'journal.created', `Journal entry created${entry.symbol ? ` for ${entry.symbol}` : ''}.`, entry);
    return entry;
  }

  updateJournalEntry(id: string, input: Partial<Omit<JournalRecord, 'id' | 'createdAt' | 'updatedAt'>>) {
    const current = this.journalById(id);
    if (!current) throw new Error('Journal entry not found.');
    const next: JournalRecord = {
      ...current,
      ...input,
      symbol: input.symbol !== undefined ? cleanSymbol(input.symbol) : current.symbol,
      tags: input.tags !== undefined ? cleanTags(input.tags) : current.tags,
      note: input.note !== undefined ? input.note.trim() : current.note,
      mistake: input.mistake !== undefined ? emptyToUndefined(input.mistake) : current.mistake,
      lesson: input.lesson !== undefined ? emptyToUndefined(input.lesson) : current.lesson,
      grade: input.grade !== undefined ? emptyToUndefined(input.grade) : current.grade,
      realizedPnl: input.realizedPnl !== undefined ? finiteOrNull(input.realizedPnl) ?? undefined : current.realizedPnl,
      updatedAt: now()
    };
    this.db.prepare(`
      update journal_entries set symbol = ?, entry_type = ?, tags_json = ?, note = ?, mistake = ?, lesson = ?, grade = ?, realized_pnl = ?, updated_at = ?
      where id = ?
    `).run(next.symbol ?? null, next.entryType, JSON.stringify(next.tags), next.note, next.mistake ?? null, next.lesson ?? null, next.grade ?? null, next.realizedPnl ?? null, next.updatedAt, id);
    this.audit('info', 'journal.updated', `Journal entry updated${next.symbol ? ` for ${next.symbol}` : ''}.`, next);
    return next;
  }

  deleteJournalEntry(id: string) {
    const current = this.journalById(id);
    if (!current) return false;
    this.db.prepare('delete from journal_entries where id = ?').run(id);
    this.audit('info', 'journal.deleted', 'Journal entry deleted.', { id });
    return true;
  }

  savedReports(limit = 100): SavedReport[] {
    const rows = this.db.prepare('select * from saved_reports order by created_at desc limit ?').all(limit) as any[];
    return rows.map((row) => this.reportFromRow(row));
  }

  saveReport(input: {
    id?: string;
    kind: SavedReport['kind'];
    title: string;
    status?: SavedReport['status'];
    source: string;
    freshness: DataFreshness;
    summary: string;
    html: string;
    payload?: unknown;
  }) {
    const timestamp = now();
    const report: SavedReport = {
      id: input.id ?? crypto.randomUUID(),
      kind: input.kind,
      title: input.title.trim(),
      status: input.status ?? 'ready',
      source: input.source,
      freshness: input.freshness,
      summary: input.summary.trim(),
      html: input.html,
      payload: input.payload ?? {},
      createdAt: timestamp,
      updatedAt: timestamp
    };
    this.db.prepare(`
      insert into saved_reports (id, kind, title, status, source, freshness, summary, html, payload_json, created_at, updated_at)
      values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
      on conflict(id) do update set
        kind = excluded.kind,
        title = excluded.title,
        status = excluded.status,
        source = excluded.source,
        freshness = excluded.freshness,
        summary = excluded.summary,
        html = excluded.html,
        payload_json = excluded.payload_json,
        updated_at = excluded.updated_at
    `).run(report.id, report.kind, report.title, report.status, report.source, report.freshness, report.summary, report.html, JSON.stringify(report.payload), report.createdAt, report.updatedAt);
    this.audit('info', 'report.saved', `${report.title} saved.`, { id: report.id, kind: report.kind });
    return report;
  }

  reportById(id: string): SavedReport | null {
    const row = this.db.prepare('select * from saved_reports where id = ?').get(id) as any;
    return row ? this.reportFromRow(row) : null;
  }

  upsertCandle(candle: CandleRecord) {
    this.db.prepare(`
      insert into candles (symbol, asset_type, timeframe, timestamp, open, high, low, close, volume, provider, fetched_at, freshness, fallback, synthetic)
      values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
      on conflict(symbol, timeframe, timestamp) do update set
        open = excluded.open,
        high = excluded.high,
        low = excluded.low,
        close = excluded.close,
        volume = excluded.volume,
        provider = excluded.provider,
        fetched_at = excluded.fetched_at,
        freshness = excluded.freshness,
        fallback = excluded.fallback,
        synthetic = excluded.synthetic
    `).run(candle.symbol, candle.assetType, candle.timeframe, candle.timestamp, candle.open, candle.high, candle.low, candle.close, candle.volume, candle.provider, candle.fetchedAt, candle.freshness, candle.fallback ? 1 : 0, candle.synthetic ? 1 : 0);
  }

  candles(symbol: string, timeframe = '1d', limit = 180): CandleRecord[] {
    const rows = this.db.prepare(`
      select * from candles where symbol = ? and timeframe = ? order by timestamp desc limit ?
    `).all(cleanSymbol(symbol) ?? symbol.toUpperCase(), timeframe, limit) as any[];
    return rows.reverse().map((row) => this.candleFromRow(row));
  }

  watchlists(): WatchlistRecord[] {
    const rows = this.db.prepare('select * from watchlists order by updated_at desc').all() as any[];
    return rows.map((row) => this.watchlistFromRow(row));
  }

  createWatchlist(input: { name: string; role?: string; tags?: string[]; symbols?: string[] }) {
    const timestamp = now();
    const watchlist: WatchlistRecord = {
      id: crypto.randomUUID(),
      name: input.name.trim(),
      role: input.role?.trim() || 'Custom research watchlist',
      tags: cleanTags(input.tags),
      symbols: cleanSymbols(input.symbols),
      createdAt: timestamp,
      updatedAt: timestamp
    };
    this.db.prepare(`
      insert into watchlists (id, name, role, tags_json, symbols_json, created_at, updated_at)
      values (?, ?, ?, ?, ?, ?, ?)
    `).run(watchlist.id, watchlist.name, watchlist.role, JSON.stringify(watchlist.tags), JSON.stringify(watchlist.symbols), watchlist.createdAt, watchlist.updatedAt);
    this.recordNotification({ severity: 'info', source: 'Watchlists', title: 'Watchlist Created', detail: `${watchlist.name} is ready for symbol tracking.` });
    this.audit('info', 'watchlist.created', `${watchlist.name} watchlist created.`, watchlist);
    return watchlist;
  }

  updateWatchlist(id: string, input: Partial<Pick<WatchlistRecord, 'name' | 'role' | 'tags' | 'symbols'>>) {
    const current = this.watchlistById(id);
    if (!current) throw new Error('Watchlist not found.');
    const next: WatchlistRecord = {
      ...current,
      name: input.name !== undefined ? input.name.trim() : current.name,
      role: input.role !== undefined ? input.role.trim() : current.role,
      tags: input.tags !== undefined ? cleanTags(input.tags) : current.tags,
      symbols: input.symbols !== undefined ? cleanSymbols(input.symbols) : current.symbols,
      updatedAt: now()
    };
    this.db.prepare(`
      update watchlists set name = ?, role = ?, tags_json = ?, symbols_json = ?, updated_at = ? where id = ?
    `).run(next.name, next.role, JSON.stringify(next.tags), JSON.stringify(next.symbols), next.updatedAt, id);
    this.audit('info', 'watchlist.updated', `${next.name} watchlist updated.`, next);
    return next;
  }

  deleteWatchlist(id: string) {
    const current = this.watchlistById(id);
    if (!current) return false;
    this.db.prepare('delete from watchlists where id = ?').run(id);
    this.audit('info', 'watchlist.deleted', `${current.name} watchlist deleted.`, current);
    return true;
  }

  savedStrategies(): StrategyRecord[] {
    const rows = this.db.prepare('select * from saved_strategies order by updated_at desc').all() as any[];
    return rows.map((row) => this.strategyFromRow(row));
  }

  saveStrategy(input: { id?: string; name: string; description?: string; rules?: string[]; preset?: string; enabled?: boolean }) {
    const existing = input.id ? this.strategyById(input.id) : null;
    const timestamp = now();
    const strategy: StrategyRecord = {
      id: input.id ?? crypto.randomUUID(),
      name: input.name.trim(),
      description: input.description?.trim() ?? existing?.description ?? 'Rule-based paper strategy.',
      rules: cleanRules(input.rules ?? existing?.rules ?? []),
      preset: input.preset?.trim() ?? existing?.preset ?? 'custom',
      enabled: input.enabled ?? existing?.enabled ?? false,
      createdAt: existing?.createdAt ?? timestamp,
      updatedAt: timestamp
    };
    this.db.prepare(`
      insert into saved_strategies (id, name, description, rules_json, preset, enabled, created_at, updated_at)
      values (?, ?, ?, ?, ?, ?, ?, ?)
      on conflict(id) do update set
        name = excluded.name,
        description = excluded.description,
        rules_json = excluded.rules_json,
        preset = excluded.preset,
        enabled = excluded.enabled,
        updated_at = excluded.updated_at
    `).run(strategy.id, strategy.name, strategy.description, JSON.stringify(strategy.rules), strategy.preset, strategy.enabled ? 1 : 0, strategy.createdAt, strategy.updatedAt);
    this.recordNotification({ severity: 'info', source: 'Strategy Lab', title: 'Strategy Saved', detail: `${strategy.name} is ready for paper review.` });
    this.audit('info', 'strategy.saved', `${strategy.name} strategy saved.`, strategy);
    return strategy;
  }

  deleteStrategy(id: string) {
    const current = this.strategyById(id);
    if (!current) return false;
    this.db.prepare('delete from saved_strategies where id = ?').run(id);
    this.audit('info', 'strategy.deleted', `${current.name} strategy deleted.`, current);
    return true;
  }

  workspaceLayouts(): WorkspaceLayoutRecord[] {
    const rows = this.db.prepare('select * from workspace_layouts order by updated_at desc').all() as any[];
    return rows.map((row) => this.layoutFromRow(row));
  }

  saveWorkspaceLayout(input: { id?: string; name: string; mode: WorkspaceLayoutRecord['mode']; config?: unknown }) {
    const existing = input.id ? this.layoutById(input.id) : null;
    const timestamp = now();
    const layout: WorkspaceLayoutRecord = {
      id: input.id ?? crypto.randomUUID(),
      name: input.name.trim(),
      mode: input.mode,
      config: input.config ?? existing?.config ?? {},
      createdAt: existing?.createdAt ?? timestamp,
      updatedAt: timestamp
    };
    this.db.prepare(`
      insert into workspace_layouts (id, name, mode, config_json, created_at, updated_at)
      values (?, ?, ?, ?, ?, ?)
      on conflict(id) do update set
        name = excluded.name,
        mode = excluded.mode,
        config_json = excluded.config_json,
        updated_at = excluded.updated_at
    `).run(layout.id, layout.name, layout.mode, JSON.stringify(layout.config), layout.createdAt, layout.updatedAt);
    this.audit('info', 'layout.saved', `${layout.name} web layout saved.`, layout);
    return layout;
  }

  notifications(limit = 100): NotificationRecord[] {
    const rows = this.db.prepare('select * from notifications order by created_at desc limit ?').all(limit) as any[];
    return rows.map((row) => this.notificationFromRow(row));
  }

  recordNotification(input: { severity: AlertSeverity; source: string; title: string; detail: string; relatedSymbol?: string }) {
    const notification: NotificationRecord = {
      id: crypto.randomUUID(),
      severity: input.severity,
      source: input.source,
      title: input.title,
      detail: input.detail,
      relatedSymbol: cleanSymbol(input.relatedSymbol),
      createdAt: now()
    };
    this.db.prepare(`
      insert into notifications (id, severity, source, title, detail, related_symbol, created_at)
      values (?, ?, ?, ?, ?, ?, ?)
    `).run(notification.id, notification.severity, notification.source, notification.title, notification.detail, notification.relatedSymbol ?? null, notification.createdAt);
    return notification;
  }

  markNotificationRead(id: string) {
    this.db.prepare('update notifications set read_at = ? where id = ? and read_at is null').run(now(), id);
  }

  updatePaperCash(cash: number) {
    const value = Math.max(0, cash);
    this.db.prepare('update account set cash = ?, updated_at = ? where id = ?').run(value, now(), 'paper');
    this.recordNotification({ severity: 'info', source: 'Portfolio', title: 'Paper Cash Updated', detail: `Paper cash model set to ${value.toFixed(2)}.` });
    this.audit('info', 'portfolio.cash_updated', 'Paper cash model updated.', { cash: value });
  }

  resetPaperAccount() {
    this.db.exec('delete from positions; delete from orders;');
    this.db.prepare(
      'update account set cash = ?, starting_equity = ?, equity_high = ?, realized_pnl = ?, day_start_equity = ?, updated_at = ? where id = ?'
    ).run(100000, 100000, 100000, 0, 100000, now(), 'paper');
    this.audit('warning', 'account.reset', 'Paper account reset.');
  }

  updateStrategyPerformance(strategy: string, pnl: number, isWin: boolean) {
    const existing = this.db.prepare('select * from strategy_performance where strategy = ?').get(strategy) as any;
    if (existing) {
      const newTrades = existing.total_trades + 1;
      const newWins = existing.winning_trades + (isWin ? 1 : 0);
      const newPnl = existing.total_pnl + pnl;
      const newAvgPnl = newPnl / newTrades;
      const newWinRate = newWins / newTrades;
      this.db.prepare('update strategy_performance set total_trades = ?, winning_trades = ?, total_pnl = ?, avg_pnl = ?, win_rate = ?, last_updated = ? where strategy = ?')
        .run(newTrades, newWins, newPnl, newAvgPnl, newWinRate, now(), strategy);
    } else {
      this.db.prepare('insert into strategy_performance (strategy, total_trades, winning_trades, total_pnl, avg_pnl, win_rate, last_updated) values (?, 1, ?, ?, ?, ?, ?)')
        .run(strategy, isWin ? 1 : 0, pnl, pnl, isWin ? 1 : 0, now());
    }
  }

  getStrategyPerformance() {
    return this.db.prepare('select * from strategy_performance order by total_pnl desc').all() as any[];
  }

  storeHistoricalPrice(symbol: string, timestamp: number, price: number, volume: number) {
    this.db.prepare('insert or replace into historical_prices (symbol, timestamp, price, volume) values (?, ?, ?, ?)')
      .run(symbol, timestamp, price, volume);
  }

  getHistoricalPrices(symbol: string, startTime: number, endTime: number): { timestamp: number; price: number; volume: number }[] {
    return this.db.prepare('select timestamp, price, volume from historical_prices where symbol = ? and timestamp between ? and ? order by timestamp')
      .all(symbol, startTime, endTime) as any[];
  }

  createAuthUser(email: string, password: string, workspaceName: string, deviceLabel: string, rememberDevice: boolean): AuthResult {
    const normalizedEmail = normalizeEmail(email);
    if (this.authUserByEmail(normalizedEmail)) {
      throw new Error('An Auralith workspace already exists for this email.');
    }
    const timestamp = now();
    const id = crypto.randomUUID();
    this.db.prepare(`
      insert into auth_users (id, email, workspace_name, password_hash, created_at, updated_at, last_login_at)
      values (?, ?, ?, ?, ?, ?, ?)
    `).run(id, normalizedEmail, workspaceName.trim(), hashPassword(password), timestamp, timestamp, timestamp);
    const user = this.authUserByEmail(normalizedEmail);
    if (!user) throw new Error('Could not create Auralith workspace.');
    this.audit('info', 'auth.registered', `Auralith account created for ${normalizedEmail}.`, { userId: id });
    return this.createAuthSession(user, deviceLabel, rememberDevice);
  }

  loginAuthUser(email: string, password: string, deviceLabel: string, rememberDevice: boolean): AuthResult {
    const normalizedEmail = normalizeEmail(email);
    const row = this.db.prepare('select * from auth_users where email = ?').get(normalizedEmail) as any;
    if (!row || !verifyPassword(password, row.password_hash)) {
      this.audit('warning', 'auth.login_failed', `Failed sign-in attempt for ${normalizedEmail}.`);
      throw new Error('Email or password is incorrect.');
    }
    const timestamp = now();
    this.db.prepare('update auth_users set last_login_at = ?, updated_at = ? where id = ?').run(timestamp, timestamp, row.id);
    const user = this.authUserFromRow({ ...row, last_login_at: timestamp });
    this.audit('info', 'auth.login', `Auralith account signed in for ${normalizedEmail}.`, { userId: user.id });
    return this.createAuthSession(user, deviceLabel, rememberDevice);
  }

  authUserForToken(token: string): AuthUser | null {
    const tokenHash = hashSessionToken(token);
    const row = this.db.prepare(`
      select u.* from auth_sessions s
      join auth_users u on u.id = s.user_id
      where s.token_hash = ? and s.revoked_at is null and s.expires_at > ?
    `).get(tokenHash, now()) as any;
    return row ? this.authUserFromRow(row) : null;
  }

  authSessionsForToken(token: string): AuthSession[] {
    const user = this.authUserForToken(token);
    if (!user) return [];
    const rows = this.db.prepare('select * from auth_sessions where user_id = ? order by created_at desc limit 20').all(user.id) as any[];
    return rows.map((row) => this.authSessionFromRow(row));
  }

  revokeAuthSession(token: string) {
    const tokenHash = hashSessionToken(token);
    this.db.prepare('update auth_sessions set revoked_at = ? where token_hash = ? and revoked_at is null').run(now(), tokenHash);
    this.audit('info', 'auth.logout', 'Auralith web session signed out.');
  }

  revokeAllAuthSessionsForToken(token: string) {
    const user = this.authUserForToken(token);
    if (!user) return 0;
    const result = this.db.prepare('update auth_sessions set revoked_at = ? where user_id = ? and revoked_at is null').run(now(), user.id) as any;
    this.audit('warning', 'auth.logout_all', 'All Auralith web sessions were revoked.', { userId: user.id });
    return Number(result?.changes ?? 0);
  }

  private alertById(id: string): AlertRecord | null {
    const row = this.db.prepare('select * from alerts where id = ?').get(id) as any;
    return row ? this.alertFromRow(row) : null;
  }

  private alertFromRow(row: any): AlertRecord {
    return {
      id: row.id,
      type: row.type,
      symbol: row.symbol ?? undefined,
      threshold: row.threshold ?? undefined,
      percentMove: row.percent_move ?? undefined,
      severity: row.severity,
      enabled: Boolean(row.enabled),
      acknowledgedAt: row.acknowledged_at ?? undefined,
      snoozedUntil: row.snoozed_until ?? undefined,
      lastTriggeredAt: row.last_triggered_at ?? undefined,
      note: row.note ?? '',
      createdAt: row.created_at,
      updatedAt: row.updated_at
    };
  }

  private alertHistoryFromRow(row: any): AlertHistoryRecord {
    return {
      id: row.id,
      alertId: row.alert_id ?? undefined,
      createdAt: row.created_at,
      severity: row.severity,
      symbol: row.symbol ?? undefined,
      title: row.title,
      detail: row.detail,
      acknowledgedAt: row.acknowledged_at ?? undefined
    };
  }

  private journalById(id: string): JournalRecord | null {
    const row = this.db.prepare('select * from journal_entries where id = ?').get(id) as any;
    return row ? this.journalFromRow(row) : null;
  }

  private journalFromRow(row: any): JournalRecord {
    return {
      id: row.id,
      symbol: row.symbol ?? undefined,
      entryType: row.entry_type,
      tags: parseTags(row.tags_json),
      note: row.note,
      mistake: row.mistake ?? undefined,
      lesson: row.lesson ?? undefined,
      grade: row.grade ?? undefined,
      realizedPnl: row.realized_pnl ?? undefined,
      createdAt: row.created_at,
      updatedAt: row.updated_at
    };
  }

  private reportFromRow(row: any): SavedReport {
    return {
      id: row.id,
      kind: row.kind,
      title: row.title,
      status: row.status,
      source: row.source,
      freshness: row.freshness,
      summary: row.summary,
      html: row.html,
      payload: parseJson(row.payload_json, {}),
      createdAt: row.created_at,
      updatedAt: row.updated_at
    };
  }

  private candleFromRow(row: any): CandleRecord {
    return {
      symbol: row.symbol,
      assetType: row.asset_type,
      timeframe: row.timeframe,
      timestamp: row.timestamp,
      open: row.open,
      high: row.high,
      low: row.low,
      close: row.close,
      volume: row.volume,
      provider: row.provider,
      fetchedAt: row.fetched_at,
      freshness: row.freshness,
      fallback: Boolean(row.fallback),
      synthetic: Boolean(row.synthetic)
    };
  }

  private watchlistById(id: string): WatchlistRecord | null {
    const row = this.db.prepare('select * from watchlists where id = ?').get(id) as any;
    return row ? this.watchlistFromRow(row) : null;
  }

  private watchlistFromRow(row: any): WatchlistRecord {
    return {
      id: row.id,
      name: row.name,
      role: row.role,
      tags: parseTags(row.tags_json),
      symbols: cleanSymbols(parseJson(row.symbols_json, [])),
      createdAt: row.created_at,
      updatedAt: row.updated_at
    };
  }

  private strategyById(id: string): StrategyRecord | null {
    const row = this.db.prepare('select * from saved_strategies where id = ?').get(id) as any;
    return row ? this.strategyFromRow(row) : null;
  }

  private strategyFromRow(row: any): StrategyRecord {
    return {
      id: row.id,
      name: row.name,
      description: row.description,
      rules: cleanRules(parseJson(row.rules_json, [])),
      preset: row.preset,
      enabled: Boolean(row.enabled),
      createdAt: row.created_at,
      updatedAt: row.updated_at
    };
  }

  private layoutById(id: string): WorkspaceLayoutRecord | null {
    const row = this.db.prepare('select * from workspace_layouts where id = ?').get(id) as any;
    return row ? this.layoutFromRow(row) : null;
  }

  private layoutFromRow(row: any): WorkspaceLayoutRecord {
    return {
      id: row.id,
      name: row.name,
      mode: row.mode,
      config: parseJson(row.config_json, {}),
      createdAt: row.created_at,
      updatedAt: row.updated_at
    };
  }

  private notificationFromRow(row: any): NotificationRecord {
    return {
      id: row.id,
      severity: row.severity,
      source: row.source,
      title: row.title,
      detail: row.detail,
      relatedSymbol: row.related_symbol ?? undefined,
      readAt: row.read_at ?? undefined,
      snoozedUntil: row.snoozed_until ?? undefined,
      createdAt: row.created_at
    };
  }

  private seedWebWorkflows() {
    if (!this.db.prepare('select id from workspace_layouts limit 1').get()) {
      const layouts: Array<[WorkspaceLayoutRecord['mode'], string]> = [
        ['daily', 'Daily Dashboard'],
        ['market-open', 'Market Open'],
        ['research', 'Research'],
        ['portfolio-review', 'Portfolio Review'],
        ['strategy-testing', 'Strategy Testing'],
        ['mobile-compact', 'Mobile Compact']
      ];
      for (const [mode, name] of layouts) {
        this.saveWorkspaceLayout({ name, mode, config: { panels: [], density: mode === 'mobile-compact' ? 'comfortable' : 'dense' } });
      }
    }
    if (!this.db.prepare('select id from saved_strategies limit 1').get()) {
      this.saveStrategy({
        name: 'Momentum Confirmation',
        description: 'Paper setup preset using moving averages, RSI, volume confirmation, and risk/reward gates.',
        preset: 'momentum',
        enabled: false,
        rules: ['Price above 20 EMA', 'Price above 50 SMA', 'RSI between 50 and 70', 'Volume above average', 'Risk/reward >= 2R']
      });
      this.saveStrategy({
        name: 'Volatility Pullback',
        description: 'Paper review preset for volatility expansion followed by controlled pullback confirmation.',
        preset: 'volatility',
        enabled: false,
        rules: ['ATR expansion above baseline', 'Price holds support', 'Market regime not defensive', 'Stop below swing low']
      });
    }
    if (!this.db.prepare('select id from watchlists limit 1').get()) {
      this.createWatchlist({ name: 'Core Indices', role: 'Market ribbon and regime context', tags: ['core'], symbols: ['SPY', 'QQQ', 'DIA', 'VIX'] });
      this.createWatchlist({ name: 'Research Queue', role: 'Symbols promoted from scanner and symbol research', tags: ['research'], symbols: ['AAPL', 'MSFT', 'NVDA', 'TSLA'] });
    }
  }

  private migrateMarketColumns() {
    const columns = this.db.prepare('pragma table_info(market_assets)').all() as Array<{ name: string }>;
    const names = new Set(columns.map((column) => column.name));
    if (!names.has('source')) {
      this.db.exec("alter table market_assets add column source text not null default 'simulated';");
    }
    if (!names.has('market_cap')) {
      this.db.exec('alter table market_assets add column market_cap real;');
    }
    if (!names.has('fetched_at')) {
      this.db.exec('alter table market_assets add column fetched_at text;');
    }
    if (!names.has('freshness')) {
      this.db.exec("alter table market_assets add column freshness text not null default 'fallback';");
    }
    if (!names.has('delayed')) {
      this.db.exec('alter table market_assets add column delayed integer not null default 1;');
    }
    if (!names.has('fallback')) {
      this.db.exec('alter table market_assets add column fallback integer not null default 1;');
    }
    if (!names.has('data_confidence')) {
      this.db.exec('alter table market_assets add column data_confidence integer not null default 50;');
    }
    if (!names.has('provider_error')) {
      this.db.exec('alter table market_assets add column provider_error text;');
    }
  }

  private authUserByEmail(email: string): AuthUser | null {
    const row = this.db.prepare('select * from auth_users where email = ?').get(normalizeEmail(email)) as any;
    return row ? this.authUserFromRow(row) : null;
  }

  private createAuthSession(user: AuthUser, deviceLabel: string, rememberDevice: boolean): AuthResult {
    const token = `auralith_${crypto.randomBytes(32).toString('hex')}`;
    const timestamp = now();
    const expiresAt = new Date(Date.now() + (rememberDevice ? 30 : 1) * 24 * 60 * 60 * 1000).toISOString();
    const session = {
      id: crypto.randomUUID(),
      userId: user.id,
      deviceLabel: cleanDeviceLabel(deviceLabel),
      createdAt: timestamp,
      expiresAt
    };
    this.db.prepare(`
      insert into auth_sessions (id, user_id, token_hash, device_label, created_at, expires_at)
      values (?, ?, ?, ?, ?, ?)
    `).run(session.id, user.id, hashSessionToken(token), session.deviceLabel, session.createdAt, session.expiresAt);
    return { user, session: { ...session, token } };
  }

  private authUserFromRow(row: any): AuthUser {
    return {
      id: row.id,
      email: row.email,
      workspaceName: row.workspace_name,
      createdAt: row.created_at,
      lastLoginAt: row.last_login_at ?? undefined
    };
  }

  private authSessionFromRow(row: any): AuthSession {
    return {
      id: row.id,
      userId: row.user_id,
      deviceLabel: row.device_label,
      createdAt: row.created_at,
      expiresAt: row.expires_at,
      revokedAt: row.revoked_at ?? undefined
    };
  }
}

export function now() {
  return new Date().toISOString();
}

function resolveDatabasePath(dbPath: string) {
  const resolved = path.resolve(dbPath);
  const legacy = path.resolve(LEGACY_DB);
  const defaultResolved = path.resolve(DEFAULT_DB);
  const shouldMigrate = resolved === defaultResolved && fs.existsSync(legacy) && !fs.existsSync(resolved);
  if (shouldMigrate) {
    fs.mkdirSync(path.dirname(resolved), { recursive: true });
    fs.copyFileSync(legacy, resolved);
    return { resolved, migratedLegacyDb: legacy };
  }
  return { resolved, migratedLegacyDb: null };
}

function cleanSymbol(value?: string) {
  const symbol = value?.trim().toUpperCase();
  return symbol ? symbol.replace(/\//g, '-') : undefined;
}

function cleanTags(tags?: string[]) {
  return [...new Set((tags ?? []).map((tag) => tag.trim().toLowerCase()).filter(Boolean))].slice(0, 12);
}

function cleanSymbols(symbols?: string[]) {
  return [...new Set((symbols ?? []).map((symbol) => cleanSymbol(symbol)).filter((symbol): symbol is string => Boolean(symbol)))].slice(0, 250);
}

function cleanRules(rules?: string[]) {
  return (rules ?? []).map((rule) => rule.trim()).filter(Boolean).slice(0, 40);
}

function parseTags(value: string) {
  const parsed = parseJson(value, []);
  return Array.isArray(parsed) ? parsed.map(String).filter(Boolean) : [];
}

function parseJson<T>(value: string, fallback: T): T {
  try {
    return JSON.parse(value) as T;
  } catch {
    return fallback;
  }
}

function finiteOrNull(value?: number) {
  return typeof value === 'number' && Number.isFinite(value) ? value : null;
}

function emptyToUndefined(value?: string) {
  const trimmed = value?.trim();
  return trimmed ? trimmed : undefined;
}

function defaultSeverity(type: AlertType): AlertSeverity {
  if (type === 'provider_stale' || type === 'risk_threshold') return 'warning';
  if (type === 'paper_order_update') return 'info';
  return 'info';
}

function alertDetail(alert: AlertRecord) {
  const subject = alert.symbol ? `${alert.symbol} ` : '';
  if (alert.type === 'price_above') return `${subject}price above ${alert.threshold ?? 'threshold'}.`;
  if (alert.type === 'price_below') return `${subject}price below ${alert.threshold ?? 'threshold'}.`;
  if (alert.type === 'percent_move') return `${subject}percent move exceeds ${alert.percentMove ?? 'configured'}%.`;
  if (alert.type === 'provider_stale') return 'Provider stale-data warning is active.';
  if (alert.type === 'risk_threshold') return 'Portfolio risk threshold warning is active.';
  return 'Paper order lifecycle update alert is active.';
}

function pct(value: number, base: number) {
  return base ? (value - base) / base : 0;
}

function normalizeEmail(email: string) {
  return email.trim().toLowerCase();
}

function cleanDeviceLabel(value: string) {
  return value.trim().slice(0, 140) || 'Auralith Web';
}

function hashPassword(password: string) {
  const salt = crypto.randomBytes(16).toString('hex');
  const key = crypto.scryptSync(password, salt, 64).toString('hex');
  return `scrypt:${salt}:${key}`;
}

function verifyPassword(password: string, stored: string) {
  const [method, salt, expected] = stored.split(':');
  if (method !== 'scrypt' || !salt || !expected) return false;
  const key = crypto.scryptSync(password, salt, 64);
  const expectedBuffer = Buffer.from(expected, 'hex');
  return key.length === expectedBuffer.length && crypto.timingSafeEqual(key, expectedBuffer);
}

function hashSessionToken(token: string) {
  return crypto.createHash('sha256').update(token).digest('hex');
}
