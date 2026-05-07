import { spawn } from 'node:child_process';
import { randomUUID } from 'node:crypto';
import { rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const port = 4300 + Math.floor(Math.random() * 300);
const db = join(tmpdir(), `auralith-smoke-${randomUUID().replaceAll('-', '')}.sqlite`);
const base = `http://127.0.0.1:${port}`;
const server = spawn(process.execPath, ['dist/server/index.js'], {
  env: {
    ...process.env,
    AURALITH_TRADER_PORT: String(port),
    AURALITH_TRADER_DB: db
  },
  stdio: 'ignore',
  windowsHide: true
});

const cookieJar = new Map();

try {
  await waitForHealth();
  const csrf = await request('/api/auth/csrf');
  const headers = { 'X-CSRF-Token': csrf.csrfToken };
  const email = `smoke-${randomUUID()}@auralith.local`;

  await request('/api/auth/register', {
    method: 'POST',
    headers,
    body: {
      email,
      password: 'Auralith123!',
      workspaceName: 'Smoke Workspace',
      rememberDevice: true
    }
  });

  const session = await request('/api/auth/session');
  assert(session.user.email === email, 'auth cookie session did not resolve');

  const alert = await request('/api/alerts', {
    method: 'POST',
    headers,
    body: { type: 'price_above', symbol: 'AAPL', threshold: 250, note: 'Smoke alert' }
  });
  assert(alert.alert.symbol === 'AAPL', 'alert create failed');

  const journal = await request('/api/journal', {
    method: 'POST',
    headers,
    body: { symbol: 'AAPL', entryType: 'research', tags: ['smoke'], note: 'Smoke journal note' }
  });
  assert(journal.entry.symbol === 'AAPL', 'journal create failed');

  const report = await request('/api/reports', {
    method: 'POST',
    headers,
    body: { kind: 'morning' }
  });
  assert(report.report.title === 'Morning Briefing', 'report generation failed');

  const eodReport = await request('/api/reports', {
    method: 'POST',
    headers,
    body: { kind: 'eod' }
  });
  assert(eodReport.report.title === 'End-of-Day Review', 'EOD report generation failed');

  const watchlist = await request('/api/watchlists', {
    method: 'POST',
    headers,
    body: { name: 'Smoke Watchlist', role: 'Smoke test', tags: ['smoke'], symbols: ['AAPL', 'MSFT'] }
  });
  assert(watchlist.watchlist.symbols.includes('AAPL'), 'watchlist create failed');

  const strategy = await request('/api/strategies', {
    method: 'POST',
    headers,
    body: { name: 'Smoke Strategy', description: 'Smoke strategy', preset: 'smoke', rules: ['RSI above 50'], enabled: false }
  });
  assert(strategy.strategy.name === 'Smoke Strategy', 'strategy save failed');

  const layout = await request('/api/layouts', {
    method: 'POST',
    headers,
    body: { name: 'Smoke Layout', mode: 'research', config: { panels: ['chart', 'ai'] } }
  });
  assert(layout.layout.name === 'Smoke Layout', 'layout save failed');

  const candles = await request('/api/candles/AAPL');
  assert(candles.candles.length >= 30, 'candle repository did not return OHLCV data');

  const morning = await request('/api/workflows/morning');
  assert(morning.workflow.sections.length >= 3, 'morning workflow missing sections');

  const eod = await request('/api/workflows/eod');
  assert(eod.workflow.actionChecklist.length >= 2, 'EOD workflow missing checklist');

  const timeline = await request('/api/research/timeline/AAPL');
  assert(Array.isArray(timeline.timeline), 'symbol timeline did not return an array');

  const diagnostics = await request('/api/providers/diagnostics');
  assert(diagnostics.diagnostics?.length >= 2, 'provider diagnostics did not return providers');

  const dataQuality = await request('/api/data-quality');
  assert(Number.isFinite(dataQuality.dataQuality?.confidence), 'data quality confidence missing');

  const platformHealth = await request('/api/platform/health');
  assert(platformHealth.health?.database?.integrity === 'ok', 'platform health integrity check failed');

  const backup = await request('/api/platform/backup/validate', {
    method: 'POST',
    headers
  });
  assert(backup.backup?.ok === true, 'backup validation failed');

  const scanner = await fetch(`${base}/scanner`, { headers: cookieHeader() });
  assert(scanner.ok, 'React Router fallback did not serve /scanner');

  console.log(JSON.stringify({
    ok: true,
    auth: session.user.email,
    alert: alert.alert.symbol,
    journal: journal.entry.symbol,
    report: report.report.title,
    eodReport: eodReport.report.title,
    watchlist: watchlist.watchlist.name,
    strategy: strategy.strategy.name,
    layout: layout.layout.name,
    candles: candles.candles.length,
    providers: diagnostics.diagnostics.length,
    dataConfidence: dataQuality.dataQuality.confidence,
    morningSections: morning.workflow.sections.length,
    timelineEvents: timeline.timeline.length,
    platformHealth: platformHealth.health.status,
    backupIntegrity: backup.backup.integrity,
    routeFallback: scanner.status
  }, null, 2));
} finally {
  server.kill();
  for (const suffix of ['', '-shm', '-wal']) {
    try {
      rmSync(`${db}${suffix}`, { force: true });
    } catch {
      // Temp cleanup only.
    }
  }
}

async function waitForHealth() {
  for (let attempt = 0; attempt < 40; attempt += 1) {
    try {
      const health = await request('/api/health');
      if (health.ok) return;
    } catch {
      await delay(250);
    }
  }
  throw new Error('Auralith smoke server did not become ready.');
}

async function request(path, init = {}) {
  const response = await fetch(`${base}${path}`, {
    method: init.method || 'GET',
    headers: {
      ...cookieHeader(),
      ...(init.body ? { 'Content-Type': 'application/json' } : {}),
      ...(init.headers || {})
    },
    body: init.body ? JSON.stringify(init.body) : undefined
  });
  storeCookies(response);
  if (!response.ok) {
    throw new Error(`${path} failed: ${response.status} ${await response.text()}`);
  }
  const text = await response.text();
  try {
    return JSON.parse(text);
  } catch (error) {
    throw new Error(`${path} did not return JSON: ${text.slice(0, 80)}`);
  }
}

function storeCookies(response) {
  const setCookie = response.headers.get('set-cookie');
  if (!setCookie) return;
  for (const cookie of setCookie.split(/,(?=\s*[^;=]+=[^;]+)/)) {
    const [pair] = cookie.split(';');
    const index = pair.indexOf('=');
    if (index > 0) cookieJar.set(pair.slice(0, index).trim(), pair.slice(index + 1).trim());
  }
}

function cookieHeader() {
  if (!cookieJar.size) return {};
  return { Cookie: [...cookieJar.entries()].map(([key, value]) => `${key}=${value}`).join('; ') };
}

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
