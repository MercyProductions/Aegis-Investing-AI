import fs from 'node:fs';
import path from 'node:path';
import { DatabaseSync } from 'node:sqlite';
import type { AppState } from './types.js';
import type { Store } from './store.js';
import type { ProviderDiagnostics } from './providerPipeline.js';

export interface PlatformHealthSnapshot {
  generatedAt: string;
  status: 'healthy' | 'degraded' | 'attention';
  safeMode: boolean;
  uptimeSeconds: number;
  database: {
    path: string;
    exists: boolean;
    sizeBytes: number;
    walMode: string;
    integrity: 'ok' | 'failed';
    integrityDetail: string;
    tableCount: number;
  };
  continuity: {
    quotes: number;
    journalEntries: number;
    reports: number;
    alerts: number;
    alertHistory: number;
    paperOrders: number;
    notifications: number;
  };
  providers: {
    total: number;
    failed: number;
    degraded: number;
    fallbackRequests: number;
    cacheHits: number;
  };
  trust: {
    confidence: number;
    fallbackSymbols: number;
    staleSymbols: number;
    failedSymbols: number;
    disclosure: string;
  };
  recommendations: string[];
}

export interface BackupValidationResult {
  ok: boolean;
  backupPath?: string;
  sizeBytes?: number;
  integrity?: string;
  createdAt: string;
  error?: string;
}

export function buildPlatformHealth(store: Store, state: AppState, diagnostics: ProviderDiagnostics[], startedAtMs: number): PlatformHealthSnapshot {
  const database = databaseHealth(store);
  const fallbackSymbols = state.market.filter((asset) => asset.metadata?.fallback || asset.metadata?.freshness === 'fallback').length;
  const staleSymbols = state.market.filter((asset) => asset.metadata?.freshness === 'stale').length;
  const failedSymbols = state.market.filter((asset) => asset.metadata?.freshness === 'failed' || asset.metadata?.error).length;
  const failedProviders = diagnostics.filter((provider) => provider.status === 'failed').length;
  const degradedProviders = diagnostics.filter((provider) => provider.status === 'degraded').length;
  const recommendations = [
    database.integrity !== 'ok' ? 'Run backup validation and keep the app in safe mode until SQLite integrity is clean.' : '',
    failedProviders ? `${failedProviders} provider routes are failing. Review diagnostics before relying on scanner output.` : '',
    fallbackSymbols || staleSymbols || failedSymbols ? `${fallbackSymbols + staleSymbols + failedSymbols} symbols have reduced data quality. Keep confidence conservative.` : '',
    state.core.portfolio.riskState !== 'Healthy' ? `Portfolio risk is ${state.core.portfolio.riskState}. Review concentration and drawdown before new paper plans.` : '',
    !state.core.safety.paperOnly || !state.core.safety.manualConfirmationRequired ? 'Safety settings need review. Paper-only and manual confirmation should remain enforced.' : ''
  ].filter(Boolean);
  const status = database.integrity !== 'ok' || failedSymbols || failedProviders > 1 || !state.core.safety.paperOnly
    ? 'attention'
    : fallbackSymbols || staleSymbols || degradedProviders || recommendations.length
      ? 'degraded'
      : 'healthy';

  return {
    generatedAt: new Date().toISOString(),
    status,
    safeMode: isSafeMode(),
    uptimeSeconds: Math.max(0, Math.round((Date.now() - startedAtMs) / 1000)),
    database,
    continuity: {
      quotes: state.market.length,
      journalEntries: store.journalEntries(10_000).length,
      reports: store.savedReports(10_000).length,
      alerts: store.alerts(10_000).length,
      alertHistory: store.alertHistory(10_000).length,
      paperOrders: store.orders(10_000).length,
      notifications: store.notifications(10_000).length
    },
    providers: {
      total: diagnostics.length,
      failed: failedProviders,
      degraded: degradedProviders,
      fallbackRequests: diagnostics.reduce((sum, provider) => sum + provider.fallbackCount, 0),
      cacheHits: diagnostics.reduce((sum, provider) => sum + provider.cacheHits, 0)
    },
    trust: {
      confidence: state.core.providerHealth.confidence,
      fallbackSymbols,
      staleSymbols,
      failedSymbols,
      disclosure: 'Auralith is research-first and paper-first. Health checks support trust and continuity; they do not make market predictions.'
    },
    recommendations: recommendations.length ? recommendations : ['Platform health is acceptable. Continue routine provider, backup, and report review.']
  };
}

export function createValidatedBackup(store: Store): BackupValidationResult {
  const createdAt = new Date().toISOString();
  try {
    const backupDir = path.join(path.dirname(store.dbPath), 'backups');
    fs.mkdirSync(backupDir, { recursive: true });
    const backupPath = path.join(backupDir, `auralith-${timestampForFile(createdAt)}.sqlite`);
    store.db.exec(`VACUUM INTO '${escapeSqlString(backupPath)}';`);
    const integrity = validateDatabaseFile(backupPath);
    const sizeBytes = fs.statSync(backupPath).size;
    store.audit(integrity === 'ok' ? 'info' : 'warning', 'platform.backup_validated', `Backup validation ${integrity}.`, {
      backupPath,
      sizeBytes,
      integrity
    });
    return { ok: integrity === 'ok', backupPath, sizeBytes, integrity, createdAt };
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    store.audit('error', 'platform.backup_failed', `Backup validation failed: ${message}`);
    return { ok: false, createdAt, error: message };
  }
}

function databaseHealth(store: Store): PlatformHealthSnapshot['database'] {
  const exists = fs.existsSync(store.dbPath);
  const sizeBytes = exists ? fs.statSync(store.dbPath).size : 0;
  const integrityDetail = integrityCheck(store.db);
  const walMode = pragmaValue(store.db, 'journal_mode') || 'unknown';
  const row = store.db.prepare("select count(*) as count from sqlite_master where type = 'table'").get() as { count?: number } | undefined;
  return {
    path: store.dbPath,
    exists,
    sizeBytes,
    walMode,
    integrity: integrityDetail === 'ok' ? 'ok' : 'failed',
    integrityDetail,
    tableCount: Number(row?.count ?? 0)
  };
}

function validateDatabaseFile(dbPath: string) {
  const db = new DatabaseSync(dbPath);
  try {
    return integrityCheck(db);
  } finally {
    db.close();
  }
}

function integrityCheck(db: DatabaseSync) {
  const row = db.prepare('pragma integrity_check').get() as Record<string, string> | undefined;
  return String(Object.values(row ?? {})[0] ?? 'failed');
}

function pragmaValue(db: DatabaseSync, pragma: string) {
  const row = db.prepare(`pragma ${pragma}`).get() as Record<string, string> | undefined;
  return String(Object.values(row ?? {})[0] ?? '');
}

function isSafeMode() {
  return process.env.AURALITH_SAFE_MODE === '1' || process.env.AURALITH_SAFE_MODE?.toLowerCase() === 'true';
}

function timestampForFile(value: string) {
  return value.replace(/[-:]/g, '').replace(/\..+$/, '').replace('T', '-');
}

function escapeSqlString(value: string) {
  return value.replace(/'/g, "''");
}
