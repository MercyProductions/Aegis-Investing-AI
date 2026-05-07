import type { AssetClass, DataFreshness, MarketAsset } from './types.js';
import { now } from './store.js';
import { STOCK_UNIVERSE } from './universe.js';
import { ProviderPipeline } from './providerPipeline.js';

const STOCK_UNIVERSE_TTL_MS = 12 * 60 * 60 * 1000;
const NASDAQ_LISTED_URL = 'https://www.nasdaqtrader.com/dynamic/SymDir/nasdaqlisted.txt';
const OTHER_LISTED_URL = 'https://www.nasdaqtrader.com/dynamic/SymDir/otherlisted.txt';
export const marketProviderPipeline = new ProviderPipeline();

interface CoinGeckoMarket {
  id: string;
  symbol: string;
  name: string;
  current_price: number;
  market_cap?: number;
  total_volume?: number;
  high_24h?: number;
  low_24h?: number;
  price_change_percentage_24h?: number;
  price_change_percentage_1h_in_currency?: number;
  last_updated?: string;
}

let stockUniverseCache: { expiresAt: number; symbols: string[] } | null = null;
let stockScanOffset = 0;

export async function fetchRealMarketAssets(): Promise<MarketAsset[]> {
  const [crypto, stocks] = await Promise.allSettled([fetchCryptoMarkets(), fetchStockMarkets()]);
  const assets: MarketAsset[] = [];
  if (crypto.status === 'fulfilled') assets.push(...crypto.value);
  if (stocks.status === 'fulfilled') assets.push(...stocks.value);
  return assets;
}

export async function fetchCryptoMarkets(): Promise<MarketAsset[]> {
  const pageCount = clampInteger(Number(process.env.AURALITH_CRYPTO_PAGES || process.env.AEGIS_CRYPTO_PAGES || 4), 1, 8);
  return (await marketProviderPipeline.cryptoMarkets(pageCount)).data;
}

async function fetchCoinGeckoPage(page: number): Promise<CoinGeckoMarket[]> {
  const url =
    `https://api.coingecko.com/api/v3/coins/markets?vs_currency=usd&order=volume_desc&per_page=250&page=${page}&sparkline=false&price_change_percentage=1h,24h,7d`;
  const response = await fetchWithTimeout(url, 20000);
  if (!response.ok) throw new Error(`CoinGecko returned ${response.status}`);
  return (await response.json()) as CoinGeckoMarket[];
}

function mapCoinGeckoMarket(row: CoinGeckoMarket, seen: Set<string>): MarketAsset {
  const change = (row.price_change_percentage_24h ?? 0) / 100;
  const previousClose = row.current_price / (1 + change || 1);
  const high = row.high_24h ?? row.current_price;
  const low = row.low_24h ?? row.current_price;
  const volatility = Math.max(0.001, Math.min(0.25, Math.abs(high - low) / row.current_price));
  const oneHour = (row.price_change_percentage_1h_in_currency ?? 0) / 100;
  const momentum = clamp(oneHour * 10 + change * 2, -1, 1);
  const baseSymbol = `${row.symbol.toUpperCase()}-USD`;
  const symbol = seen.has(baseSymbol) ? `${row.symbol.toUpperCase()}-${normalizeSymbol(row.id).slice(0, 10)}-USD` : baseSymbol;
  seen.add(symbol);
  seen.add(baseSymbol);
  return {
    symbol,
    name: row.name,
    assetClass: 'crypto',
    price: row.current_price,
    previousClose,
    changePct: change,
    momentum,
    volatility,
    volume: row.total_volume ?? 0,
    source: 'CoinGecko',
    marketCap: row.market_cap,
    updatedAt: row.last_updated ?? now(),
    metadata: providerMetadata('CoinGecko', row.last_updated ?? now(), 'live', 86)
  } satisfies MarketAsset;
}

export async function fetchStockMarkets(symbols?: string[]): Promise<MarketAsset[]> {
  const scanSymbols = symbols ?? (await getStockScanSymbols());
  const concurrency = clampInteger(Number(process.env.AURALITH_STOCK_QUOTE_CONCURRENCY || process.env.AEGIS_STOCK_QUOTE_CONCURRENCY || 20), 1, 50);
  const results: MarketAsset[] = [];
  for (let index = 0; index < scanSymbols.length; index += concurrency) {
    const batch = scanSymbols.slice(index, index + concurrency);
    const quotes = await marketProviderPipeline.quoteBatch(batch, 'stock');
    results.push(...quotes.map((quote) => quote.data));
  }
  return results;
}

export async function fetchStockQuote(symbol: string): Promise<MarketAsset | null> {
  return (await marketProviderPipeline.quote(normalizeSymbol(symbol), 'stock')).data;
}

export function providerDiagnostics() {
  return marketProviderPipeline.diagnostics();
}

export function dataQualityForAssets(assets: MarketAsset[]) {
  return marketProviderPipeline.dataQuality(assets);
}

export function fetchProviderCandles(symbol: string, timeframe = '1d', assetClass?: AssetClass) {
  return marketProviderPipeline.candles(normalizeSymbol(symbol), timeframe, assetClass);
}

async function getStockScanSymbols(): Promise<string[]> {
  const staticSymbols = STOCK_UNIVERSE;
  const mode = (process.env.AURALITH_STOCK_UNIVERSE_MODE || process.env.AEGIS_STOCK_UNIVERSE_MODE || 'listed').toLowerCase();
  if (mode === 'static') return staticSymbols;

  const fullUniverse = await fetchListedStockUniverse().catch(() => staticSymbols);
  const priority = new Set(staticSymbols);
  const broadUniverse = fullUniverse.filter((symbol) => !priority.has(symbol));
  const batchSize = clampInteger(Number(process.env.AURALITH_STOCK_SCAN_BATCH || process.env.AEGIS_STOCK_SCAN_BATCH || 180), 25, 1000);
  const rotatingBatch = takeCircularBatch(broadUniverse, stockScanOffset, batchSize);
  stockScanOffset = broadUniverse.length ? (stockScanOffset + batchSize) % broadUniverse.length : 0;
  return uniqueSymbols([...staticSymbols, ...rotatingBatch]);
}

export async function fetchListedStockUniverse(): Promise<string[]> {
  if (stockUniverseCache && Date.now() < stockUniverseCache.expiresAt) return stockUniverseCache.symbols;
  const [nasdaq, other] = await Promise.all([
    fetchText(NASDAQ_LISTED_URL, 20000),
    fetchText(OTHER_LISTED_URL, 20000)
  ]);
  const symbols = uniqueSymbols([
    ...parseSymbolDirectory(nasdaq, 'Symbol', 'Security Name', 'Test Issue'),
    ...parseSymbolDirectory(other, 'ACT Symbol', 'Security Name', 'Test Issue')
  ]);
  if (symbols.length < STOCK_UNIVERSE.length) return STOCK_UNIVERSE;
  stockUniverseCache = { expiresAt: Date.now() + STOCK_UNIVERSE_TTL_MS, symbols };
  return symbols;
}

async function fetchYahooChartQuote(symbol: string): Promise<MarketAsset | null> {
  const url = `https://query1.finance.yahoo.com/v8/finance/chart/${encodeURIComponent(symbol)}?range=1d&interval=1m`;
  const response = await fetchWithTimeout(url, 10000);
  if (!response.ok) return fetchStooqQuote(symbol).catch(() => null);
  const payload = (await response.json()) as any;
  const result = payload?.chart?.result?.[0];
  const meta = result?.meta;
  if (!Number.isFinite(Number(meta?.regularMarketPrice))) return fetchStooqQuote(symbol).catch(() => null);
  const price = Number(meta.regularMarketPrice);
  const previousClose = Number(meta.previousClose ?? meta.chartPreviousClose ?? price);
  const high = Number(meta.regularMarketDayHigh ?? price);
  const low = Number(meta.regularMarketDayLow ?? price);
  const changePct = previousClose ? (price - previousClose) / previousClose : 0;
  return {
    symbol,
    name: String(meta.longName ?? meta.shortName ?? symbol),
    assetClass: 'stock',
    price,
    previousClose,
    changePct,
    momentum: clamp(changePct * 8, -1, 1),
    volatility: Math.max(0.001, Math.min(0.15, Math.abs(high - low) / price)),
    volume: Number(meta.regularMarketVolume ?? 0),
    source: 'Yahoo Chart',
    updatedAt: meta.regularMarketTime ? new Date(Number(meta.regularMarketTime) * 1000).toISOString() : now(),
    metadata: providerMetadata('Yahoo Chart', meta.regularMarketTime ? new Date(Number(meta.regularMarketTime) * 1000).toISOString() : now(), 'delayed', 74)
  };
}

async function fetchStooqQuote(symbol: string): Promise<MarketAsset | null> {
  const url = `https://stooq.com/q/l/?s=${encodeURIComponent(symbol.toLowerCase())}.us&f=sd2t2ohlcv&h&e=csv`;
  const response = await fetchWithTimeout(url, 10000);
  if (!response.ok) return null;
  const text = await response.text();
  const [, line] = text.trim().split(/\r?\n/);
  if (!line || line.includes('N/D')) return null;
  const [rawSymbol, date, time, open, high, low, close, volume] = line.split(',');
  const price = Number(close);
  const previousClose = Number(open) || price;
  if (!Number.isFinite(price)) return null;
  return {
    symbol: rawSymbol.replace('.US', ''),
    name: symbol,
    assetClass: 'stock',
    price,
    previousClose,
    changePct: previousClose ? (price - previousClose) / previousClose : 0,
    momentum: previousClose ? clamp(((price - previousClose) / previousClose) * 8, -1, 1) : 0,
    volatility: price ? Math.abs(Number(high) - Number(low)) / price : 0.001,
    volume: Number(volume) || 0,
    source: 'Stooq',
    updatedAt: new Date(`${date}T${time}Z`).toISOString(),
    metadata: providerMetadata('Stooq', new Date(`${date}T${time}Z`).toISOString(), 'fallback', 62)
  };
}

function providerMetadata(provider: string, fetchedAt: string, freshness: DataFreshness, confidence: number, error?: string) {
  return {
    provider,
    fetchedAt,
    freshness,
    delayed: freshness === 'delayed' || freshness === 'stale' || freshness === 'fallback',
    fallback: freshness === 'fallback' || freshness === 'failed',
    confidence,
    error
  };
}

function parseSymbolDirectory(text: string, symbolHeader: string, nameHeader: string, testHeader: string): string[] {
  const lines = text.split(/\r?\n/).filter(Boolean);
  const headers = lines.shift()?.split('|') ?? [];
  const symbolIndex = headers.indexOf(symbolHeader);
  const nameIndex = headers.indexOf(nameHeader);
  const testIndex = headers.indexOf(testHeader);
  if (symbolIndex < 0 || nameIndex < 0 || testIndex < 0) return [];
  const symbols: string[] = [];
  for (const line of lines) {
    if (line.startsWith('File Creation Time')) continue;
    const parts = line.split('|');
    const symbol = normalizeSymbol(parts[symbolIndex] ?? '');
    const name = parts[nameIndex] ?? '';
    const isTest = (parts[testIndex] ?? 'Y').trim().toUpperCase() !== 'N';
    if (!isTest && isTradableCommonSymbol(symbol, name)) symbols.push(symbol);
  }
  return symbols;
}

function isTradableCommonSymbol(symbol: string, name: string) {
  if (!/^[A-Z][A-Z0-9.-]{0,9}$/.test(symbol)) return false;
  if (symbol.includes('^') || symbol.includes('$') || symbol.includes('~')) return false;
  const lowerName = name.toLowerCase();
  if (/\b(warrant|right|unit|preferred|preference|depositary share)\b/.test(lowerName)) return false;
  return true;
}

async function fetchText(url: string, timeoutMs: number): Promise<string> {
  const response = await fetchWithTimeout(url, timeoutMs);
  if (!response.ok) throw new Error(`${url} returned ${response.status}`);
  return response.text();
}

async function fetchWithTimeout(url: string, timeoutMs: number): Promise<Response> {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), timeoutMs);
  try {
    return await fetch(url, {
      signal: controller.signal,
      headers: {
        'user-agent': 'AuralithMarkets/0.2 local paper research scanner'
      }
    });
  } finally {
    clearTimeout(timeout);
  }
}

function takeCircularBatch<T>(items: T[], offset: number, count: number): T[] {
  if (!items.length) return [];
  const result: T[] = [];
  for (let index = 0; index < Math.min(count, items.length); index += 1) {
    result.push(items[(offset + index) % items.length]);
  }
  return result;
}

function uniqueSymbols(symbols: string[]) {
  return [...new Set(symbols.map(normalizeSymbol).filter(Boolean))];
}

function normalizeSymbol(symbol: string) {
  return symbol.trim().toUpperCase().replace(/\//g, '-').replace(/\s+/g, '');
}

function clampInteger(value: number, min: number, max: number) {
  if (!Number.isFinite(value)) return min;
  return Math.min(max, Math.max(min, Math.round(value)));
}

function clamp(value: number, min: number, max: number) {
  return Math.min(max, Math.max(min, value));
}
