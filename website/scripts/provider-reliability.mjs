import { spawn } from 'node:child_process';
import { randomUUID } from 'node:crypto';
import { rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import WebSocket from 'ws';

const port = 4700 + Math.floor(Math.random() * 300);
const db = join(tmpdir(), `auralith-provider-${randomUUID().replaceAll('-', '')}.sqlite`);
const base = `http://127.0.0.1:${port}`;
const server = spawn(process.execPath, ['dist/server/index.js'], {
  env: {
    ...process.env,
    AURALITH_TRADER_PORT: String(port),
    AURALITH_TRADER_DB: db,
    AURALITH_STOCK_UNIVERSE_MODE: 'static',
    AURALITH_MARKET_REFRESH_MS: '600000'
  },
  stdio: 'ignore',
  windowsHide: true
});

const cookieJar = new Map();

try {
  await waitForHealth();
  const csrf = await request('/api/auth/csrf');
  const headers = { 'X-CSRF-Token': csrf.csrfToken };

  const diagnostics = await request('/api/providers/diagnostics');
  assert(Array.isArray(diagnostics.diagnostics), 'provider diagnostics did not return a provider list');
  assert(diagnostics.diagnostics.some((provider) => provider.id === 'demo'), 'demo fallback provider is missing');
  assert(diagnostics.diagnostics.some((provider) => provider.capabilities.includes('quotes')), 'quote capability is missing');
  assert(diagnostics.diagnostics.some((provider) => provider.capabilities.includes('candles')), 'candle capability is missing');

  const quality = await request('/api/data-quality');
  assert(Number.isFinite(quality.dataQuality.confidence), 'data-quality confidence is not numeric');
  assert(quality.quotes.length > 0, 'data-quality quote metadata is empty');
  assert(quality.quotes.every((quote) => quote.provider && quote.fetchedAt && quote.freshness), 'quotes must include provider/fetchedAt/freshness metadata');

  const candles = await request('/api/candles/ZZZZZZ?timeframe=1d');
  assert(candles.candles.length >= 30, 'fallback candle repository did not return enough OHLCV rows');
  assert(candles.candles.some((candle) => candle.fallback || candle.synthetic), 'fallback candle rows were not labeled');

  const quoteState = await request('/api/market/quote', {
    method: 'POST',
    headers,
    body: { symbol: 'ZZZZZZ' }
  });
  const fallbackQuote = quoteState.market.find((asset) => asset.symbol === 'ZZZZZZ');
  assert(fallbackQuote, 'fallback quote was not added to market state');
  assert(fallbackQuote.metadata?.provider, 'fallback quote did not carry provider metadata');
  assert(fallbackQuote.metadata?.freshness, 'fallback quote did not carry freshness metadata');

  const live = await waitForLiveEvent();
  assert(live.type === 'hello', 'websocket did not send the initial provider event');
  assert(live.payload?.provider?.dataQuality, 'websocket provider payload missing data quality');

  console.log(JSON.stringify({
    ok: true,
    providers: diagnostics.diagnostics.map((provider) => `${provider.name}:${provider.status}`),
    confidence: quality.dataQuality.confidence,
    candles: candles.candles.length,
    fallbackQuote: fallbackQuote.metadata,
    live: live.type
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
  throw new Error('Auralith provider test server did not become ready.');
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
  if (!response.ok) throw new Error(`${path} failed: ${response.status} ${await response.text()}`);
  return response.json();
}

function waitForLiveEvent() {
  return new Promise((resolve, reject) => {
    const socket = new WebSocket(`ws://127.0.0.1:${port}/api/live`);
    const timeout = setTimeout(() => {
      socket.close();
      reject(new Error('Timed out waiting for live provider event.'));
    }, 5000);
    socket.once('message', (data) => {
      clearTimeout(timeout);
      socket.close();
      resolve(JSON.parse(String(data)));
    });
    socket.once('error', (error) => {
      clearTimeout(timeout);
      reject(error);
    });
  });
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
