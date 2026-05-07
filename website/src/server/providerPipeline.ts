import type { AssetClass, DataFreshness, MarketAsset, MarketDataMetadata } from './types.js';
import { now } from './store.js';
import { SEEDED_CRYPTO, STOCK_UNIVERSE } from './universe.js';

export type ProviderCapability =
  | 'quotes'
  | 'candles'
  | 'fundamentals'
  | 'news'
  | 'earnings'
  | 'sec-filings'
  | 'etf-exposure'
  | 'crypto'
  | 'macro';

export interface ProviderDiagnostics {
  id: string;
  name: string;
  tier: 'primary' | 'secondary' | 'public-fallback' | 'demo-fallback';
  capabilities: ProviderCapability[];
  status: 'ready' | 'degraded' | 'failed' | 'disabled';
  lastAttemptAt?: string;
  lastSuccessAt?: string;
  lastError?: string;
  requestCount: number;
  failureCount: number;
  cacheHits: number;
  fallbackCount: number;
  averageLatencyMs: number;
  rateLimitState: 'ok' | 'limited' | 'unknown';
}

export interface ProviderDataQuality {
  provider: string;
  fetchedAt: string;
  freshness: DataFreshness;
  confidence: number;
  delayed: boolean;
  fallback: boolean;
  stale: boolean;
  synthetic: boolean;
  error?: string;
}

export interface ProviderBundle<T> {
  data: T;
  quality: ProviderDataQuality;
}

export interface HistoricalCandle {
  symbol: string;
  assetType: AssetClass;
  timeframe: string;
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  provider: string;
  fetchedAt: string;
  freshness: DataFreshness;
  fallback: boolean;
  synthetic: boolean;
}

export interface IMarketProvider {
  id: string;
  name: string;
  tier: ProviderDiagnostics['tier'];
  capabilities: ProviderCapability[];
  quote(symbol: string, assetClass?: AssetClass): Promise<ProviderBundle<MarketAsset> | null>;
  quoteBatch?(symbols: string[], assetClass?: AssetClass): Promise<Array<ProviderBundle<MarketAsset>>>;
  cryptoMarkets?(pageCount: number): Promise<ProviderBundle<MarketAsset[]> | null>;
  candles?(symbol: string, timeframe: string, assetClass?: AssetClass): Promise<ProviderBundle<HistoricalCandle[]> | null>;
  fundamentals?(symbol: string): Promise<ProviderBundle<Array<{ label: string; value: string; source: string }>> | null>;
  news?(symbol: string): Promise<ProviderBundle<Array<{ title: string; source: string; freshness: DataFreshness }>> | null>;
  earnings?(symbol: string): Promise<ProviderBundle<Array<{ label: string; value: string; source: string }>> | null>;
  secFilings?(symbol: string): Promise<ProviderBundle<Array<{ title: string; url: string; source: string }>> | null>;
  etfExposure?(symbol: string): Promise<ProviderBundle<Array<{ holder: string; weight: number; source: string }>> | null>;
  macro?(): Promise<ProviderBundle<Array<{ label: string; value: string; source: string }>> | null>;
}

type ProviderState = {
  lastAttemptAt?: string;
  lastSuccessAt?: string;
  lastError?: string;
  requestCount: number;
  failureCount: number;
  cacheHits: number;
  fallbackCount: number;
  latencyTotalMs: number;
  latencySamples: number;
  status: ProviderDiagnostics['status'];
};

export class ProviderPipeline {
  private readonly providers: IMarketProvider[];
  private readonly state = new Map<string, ProviderState>();
  private readonly quoteCache = new Map<string, { expiresAt: number; bundle: ProviderBundle<MarketAsset> }>();

  constructor(providers = defaultProviders()) {
    this.providers = providers.sort((a, b) => tierRank(a.tier) - tierRank(b.tier));
    for (const provider of this.providers) {
      this.state.set(provider.id, {
        requestCount: 0,
        failureCount: 0,
        cacheHits: 0,
        fallbackCount: 0,
        latencyTotalMs: 0,
        latencySamples: 0,
        status: 'ready'
      });
    }
  }

  async quote(symbol: string, assetClass?: AssetClass): Promise<ProviderBundle<MarketAsset>> {
    const normalized = normalizeSymbol(symbol);
    const cacheKey = `${normalized}:${assetClass ?? 'any'}`;
    const cached = this.quoteCache.get(cacheKey);
    if (cached && cached.expiresAt > Date.now()) {
      this.bumpCacheHit(cached.bundle.quality.provider);
      return cached.bundle;
    }

    const candidates = this.providers.filter((provider) => provider.capabilities.includes('quotes') && (!assetClass || assetClass !== 'crypto' || provider.capabilities.includes('crypto')));
    const errors: string[] = [];
    for (const provider of candidates) {
      try {
        const bundle = await this.runProvider(provider, () => provider.quote(normalized, assetClass));
        if (!bundle) continue;
        const normalizedBundle = normalizeQuoteBundle(bundle, provider);
        if (provider.tier.includes('fallback')) this.bumpFallback(provider.id);
        this.quoteCache.set(cacheKey, { expiresAt: Date.now() + cacheTtlMs(normalizedBundle.quality), bundle: normalizedBundle });
        return normalizedBundle;
      } catch (error) {
        errors.push(`${provider.name}: ${error instanceof Error ? error.message : String(error)}`);
      }
    }
    const fallbackProvider = this.providers.find((provider) => provider.id === 'demo');
    if (fallbackProvider) {
      const bundle = await this.runProvider(fallbackProvider, () => fallbackProvider.quote(normalized, assetClass));
      if (bundle) {
        this.bumpFallback(fallbackProvider.id);
        return normalizeQuoteBundle(withError(bundle, errors.join(' / ') || 'All market providers failed.'), fallbackProvider);
      }
    }
    throw new Error(errors.join(' / ') || `No provider could return ${normalized}.`);
  }

  async quoteBatch(symbols: string[], assetClass?: AssetClass) {
    const unique = [...new Set(symbols.map(normalizeSymbol).filter(Boolean))];
    const resolved = new Map<string, ProviderBundle<MarketAsset>>();
    const candidates = this.providers.filter(
      (provider) =>
        provider.capabilities.includes('quotes') &&
        typeof provider.quoteBatch === 'function' &&
        (!assetClass || assetClass !== 'crypto' || provider.capabilities.includes('crypto'))
    );

    for (const provider of candidates) {
      const missing = unique.filter((symbol) => !resolved.has(symbol));
      if (!missing.length) break;
      try {
        const bundles = await this.runProvider(provider, () => provider.quoteBatch!(missing, assetClass).then((items) =>
          bundle(items, metadata(provider.name, now(), provider.tier.includes('fallback') ? 'fallback' : 'delayed', provider.tier.includes('fallback') ? 52 : 72))
        ));
        for (const item of bundles?.data ?? []) {
          const normalizedBundle = normalizeQuoteBundle(item, provider);
          resolved.set(normalizedBundle.data.symbol, normalizedBundle);
          this.quoteCache.set(`${normalizedBundle.data.symbol}:${assetClass ?? 'any'}`, { expiresAt: Date.now() + cacheTtlMs(normalizedBundle.quality), bundle: normalizedBundle });
        }
      } catch {
        // Individual quote fallback below will record provider-specific failures.
      }
    }

    const results = await Promise.allSettled(unique.filter((symbol) => !resolved.has(symbol)).map((symbol) => this.quote(symbol, assetClass)));
    for (const result of results) {
      if (result.status === 'fulfilled') resolved.set(result.value.data.symbol, result.value);
    }
    return [...resolved.values()];
  }

  async candles(symbol: string, timeframe = '1d', assetClass?: AssetClass): Promise<ProviderBundle<HistoricalCandle[]>> {
    const normalized = normalizeSymbol(symbol);
    const candidates = this.providers.filter((provider) => provider.capabilities.includes('candles') && typeof provider.candles === 'function');
    const errors: string[] = [];
    for (const provider of candidates) {
      try {
        const bundle = await this.runProvider(provider, () => provider.candles?.(normalized, timeframe, assetClass) ?? Promise.resolve(null));
        if (bundle?.data?.length) return bundle;
      } catch (error) {
        errors.push(`${provider.name}: ${error instanceof Error ? error.message : String(error)}`);
      }
    }
    const demo = this.providers.find((provider) => provider.id === 'demo');
    const bundle = await this.runProvider(demo!, () => demo!.candles!(normalized, timeframe, assetClass));
    if (!bundle) throw new Error(errors.join(' / ') || `No candles returned for ${normalized}.`);
    this.bumpFallback(demo!.id);
    return withError(bundle, errors.join(' / ') || undefined);
  }

  async cryptoMarkets(pageCount = 4): Promise<ProviderBundle<MarketAsset[]>> {
    const candidates = this.providers.filter((provider) => provider.capabilities.includes('crypto') && typeof provider.cryptoMarkets === 'function');
    const errors: string[] = [];
    for (const provider of candidates) {
      try {
        const bundle = await this.runProvider(provider, () => provider.cryptoMarkets!(pageCount));
        if (bundle?.data?.length) return bundle;
      } catch (error) {
        errors.push(`${provider.name}: ${error instanceof Error ? error.message : String(error)}`);
      }
    }
    const seeded = await this.quoteBatch(SEEDED_CRYPTO.map((asset) => asset.symbol), 'crypto');
    return withError(bundle(seeded.map((item) => item.data), metadata('Demo market lab', now(), 'fallback', 45)), errors.join(' / ') || 'Crypto market provider fallback is active.');
  }

  diagnostics(): ProviderDiagnostics[] {
    return this.providers.map((provider) => {
      const state = this.state.get(provider.id)!;
      return {
        id: provider.id,
        name: provider.name,
        tier: provider.tier,
        capabilities: provider.capabilities,
        status: state.status,
        lastAttemptAt: state.lastAttemptAt,
        lastSuccessAt: state.lastSuccessAt,
        lastError: state.lastError,
        requestCount: state.requestCount,
        failureCount: state.failureCount,
        cacheHits: state.cacheHits,
        fallbackCount: state.fallbackCount,
        averageLatencyMs: state.latencySamples ? Math.round(state.latencyTotalMs / state.latencySamples) : 0,
        rateLimitState: state.lastError?.toLowerCase().includes('429') || state.lastError?.toLowerCase().includes('rate') ? 'limited' : 'ok'
      };
    });
  }

  dataQuality(assets: MarketAsset[]) {
    const counts = assets.reduce(
      (acc, asset) => {
        const freshness = asset.metadata?.freshness ?? 'fallback';
        acc[freshness] += 1;
        if (asset.metadata?.fallback) acc.fallback += freshness === 'fallback' ? 0 : 1;
        if (asset.metadata?.error) acc.failed += freshness === 'failed' ? 0 : 1;
        return acc;
      },
      { live: 0, delayed: 0, stale: 0, fallback: 0, failed: 0 } as Record<DataFreshness, number>
    );
    const confidence = assets.length ? Math.round(assets.reduce((sum, asset) => sum + (asset.metadata?.confidence ?? 50), 0) / assets.length) : 0;
    const warnings = [
      counts.fallback ? `${counts.fallback} symbols are using fallback/demo data.` : '',
      counts.stale ? `${counts.stale} symbols are stale.` : '',
      counts.failed ? `${counts.failed} symbols have provider errors.` : ''
    ].filter(Boolean);
    return {
      counts,
      confidence,
      warnings,
      generatedAt: now()
    };
  }

  private async runProvider<T>(provider: IMarketProvider, task: () => Promise<ProviderBundle<T> | null>) {
    const state = this.state.get(provider.id)!;
    const started = Date.now();
    state.requestCount += 1;
    state.lastAttemptAt = now();
    try {
      const result = await task();
      state.latencyTotalMs += Date.now() - started;
      state.latencySamples += 1;
      if (result) {
        state.lastSuccessAt = now();
        state.status = provider.tier === 'demo-fallback' ? 'degraded' : 'ready';
      }
      return result;
    } catch (error) {
      state.failureCount += 1;
      state.lastError = error instanceof Error ? error.message : String(error);
      state.status = 'failed';
      throw error;
    }
  }

  private bumpCacheHit(providerName: string) {
    const provider = this.providers.find((item) => item.name === providerName || item.id === providerName);
    if (provider) this.state.get(provider.id)!.cacheHits += 1;
  }

  private bumpFallback(providerId: string) {
    const state = this.state.get(providerId);
    if (state) state.fallbackCount += 1;
  }
}

class YahooChartProvider implements IMarketProvider {
  id = 'yahoo-chart';
  name = 'Yahoo Chart';
  tier = 'primary' as const;
  capabilities: ProviderCapability[] = ['quotes', 'candles'];

  async quote(symbol: string) {
    const url = `https://query1.finance.yahoo.com/v8/finance/chart/${encodeURIComponent(symbol)}?range=1d&interval=1m`;
    const payload = await jsonFetch(url, 10000);
    const result = payload?.chart?.result?.[0];
    const meta = result?.meta;
    if (!Number.isFinite(Number(meta?.regularMarketPrice))) return null;
    const fetchedAt = meta.regularMarketTime ? new Date(Number(meta.regularMarketTime) * 1000).toISOString() : now();
    const price = Number(meta.regularMarketPrice);
    const previousClose = Number(meta.previousClose ?? meta.chartPreviousClose ?? price);
    const high = Number(meta.regularMarketDayHigh ?? price);
    const low = Number(meta.regularMarketDayLow ?? price);
    const changePct = previousClose ? (price - previousClose) / previousClose : 0;
    return bundle({
      symbol,
      name: String(meta.longName ?? meta.shortName ?? symbol),
      assetClass: 'stock',
      price,
      previousClose,
      changePct,
      momentum: clamp(changePct * 8, -1, 1),
      volatility: Math.max(0.001, Math.min(0.15, Math.abs(high - low) / Math.max(price, 0.01))),
      volume: Number(meta.regularMarketVolume ?? 0),
      source: this.name,
      updatedAt: fetchedAt,
      metadata: metadata(this.name, fetchedAt, freshnessFromAge(fetchedAt, 'delayed'), 76)
    } satisfies MarketAsset);
  }

  async candles(symbol: string, timeframe: string) {
    const interval = timeframe === '1h' ? '1h' : '1d';
    const range = timeframe === '1h' ? '3mo' : '1y';
    const url = `https://query1.finance.yahoo.com/v8/finance/chart/${encodeURIComponent(symbol)}?range=${range}&interval=${interval}`;
    const payload = await jsonFetch(url, 15000);
    const result = payload?.chart?.result?.[0];
    const timestamps = result?.timestamp ?? [];
    const quote = result?.indicators?.quote?.[0] ?? {};
    const meta = result?.meta ?? {};
    const candles: HistoricalCandle[] = timestamps.map((time: number, index: number) => ({
      symbol,
      assetType: 'stock',
      timeframe,
      timestamp: Number(time) * 1000,
      open: Number(quote.open?.[index]),
      high: Number(quote.high?.[index]),
      low: Number(quote.low?.[index]),
      close: Number(quote.close?.[index]),
      volume: Number(quote.volume?.[index] ?? 0),
      provider: this.name,
      fetchedAt: now(),
      freshness: 'delayed',
      fallback: false,
      synthetic: false
    })).filter((candle: HistoricalCandle) => [candle.open, candle.high, candle.low, candle.close].every(Number.isFinite));
    if (!candles.length) return null;
    return bundle(candles, metadata(this.name, meta.regularMarketTime ? new Date(Number(meta.regularMarketTime) * 1000).toISOString() : now(), 'delayed', 72));
  }
}

class StooqProvider implements IMarketProvider {
  id = 'stooq';
  name = 'Stooq';
  tier = 'public-fallback' as const;
  capabilities: ProviderCapability[] = ['quotes', 'candles'];

  async quote(symbol: string) {
    const text = await textFetch(`https://stooq.com/q/l/?s=${encodeURIComponent(symbol.toLowerCase())}.us&f=sd2t2ohlcv&h&e=csv`, 10000);
    const [, line] = text.trim().split(/\r?\n/);
    if (!line || line.includes('N/D')) return null;
    const [rawSymbol, date, time, open, high, low, close, volume] = line.split(',');
    const price = Number(close);
    const previousClose = Number(open) || price;
    if (!Number.isFinite(price)) return null;
    const fetchedAt = safeIso(`${date}T${time}Z`);
    const meta = metadata(this.name, fetchedAt, 'fallback', 62);
    return bundle({
      symbol: rawSymbol.replace('.US', ''),
      name: symbol,
      assetClass: 'stock',
      price,
      previousClose,
      changePct: previousClose ? (price - previousClose) / previousClose : 0,
      momentum: previousClose ? clamp(((price - previousClose) / previousClose) * 8, -1, 1) : 0,
      volatility: price ? Math.abs(Number(high) - Number(low)) / price : 0.001,
      volume: Number(volume) || 0,
      source: this.name,
      updatedAt: fetchedAt,
      metadata: meta
    } satisfies MarketAsset, meta);
  }
}

class CoinGeckoProvider implements IMarketProvider {
  id = 'coingecko';
  name = 'CoinGecko';
  tier = 'primary' as const;
  capabilities: ProviderCapability[] = ['quotes', 'crypto'];

  async quote(symbol: string, assetClass?: AssetClass) {
    if (assetClass && assetClass !== 'crypto') return null;
    const id = cryptoId(symbol);
    const rows = await jsonFetch(`https://api.coingecko.com/api/v3/coins/markets?vs_currency=usd&ids=${encodeURIComponent(id)}&sparkline=false&price_change_percentage=1h,24h`, 12000) as any[];
    const row = rows[0];
    if (!row || !Number.isFinite(row.current_price)) return null;
    const change = (row.price_change_percentage_24h ?? 0) / 100;
    const previousClose = row.current_price / (1 + change || 1);
    const high = row.high_24h ?? row.current_price;
    const low = row.low_24h ?? row.current_price;
    const fetchedAt = row.last_updated ?? now();
    const meta = metadata(this.name, fetchedAt, freshnessFromAge(fetchedAt, 'live'), 86);
    return bundle({
      symbol: `${String(row.symbol).toUpperCase()}-USD`,
      name: row.name,
      assetClass: 'crypto',
      price: row.current_price,
      previousClose,
      changePct: change,
      momentum: clamp(((row.price_change_percentage_1h_in_currency ?? 0) / 100) * 10 + change * 2, -1, 1),
      volatility: Math.max(0.001, Math.min(0.25, Math.abs(high - low) / row.current_price)),
      volume: row.total_volume ?? 0,
      source: this.name,
      marketCap: row.market_cap,
      updatedAt: fetchedAt,
      metadata: meta
    } satisfies MarketAsset, meta);
  }

  async quoteBatch(symbols: string[]) {
    const ids = symbols.map(cryptoId).filter(Boolean).slice(0, 250);
    if (!ids.length) return [];
    const rows = await jsonFetch(`https://api.coingecko.com/api/v3/coins/markets?vs_currency=usd&ids=${encodeURIComponent(ids.join(','))}&sparkline=false&price_change_percentage=1h,24h`, 20000) as any[];
    return rows.flatMap((row) => {
      if (!Number.isFinite(row.current_price)) return [];
      const change = (row.price_change_percentage_24h ?? 0) / 100;
      const previousClose = row.current_price / (1 + change || 1);
      const fetchedAt = row.last_updated ?? now();
      const high = row.high_24h ?? row.current_price;
      const low = row.low_24h ?? row.current_price;
      const meta = metadata(this.name, fetchedAt, freshnessFromAge(fetchedAt, 'live'), 86);
      return [bundle({
        symbol: `${String(row.symbol).toUpperCase()}-USD`,
        name: row.name,
        assetClass: 'crypto',
        price: row.current_price,
        previousClose,
        changePct: change,
        momentum: clamp(((row.price_change_percentage_1h_in_currency ?? 0) / 100) * 10 + change * 2, -1, 1),
        volatility: Math.max(0.001, Math.min(0.25, Math.abs(high - low) / row.current_price)),
        volume: row.total_volume ?? 0,
        source: this.name,
        marketCap: row.market_cap,
        updatedAt: fetchedAt,
        metadata: meta
      } satisfies MarketAsset, meta)];
    });
  }

  async cryptoMarkets(pageCount: number) {
    const pages = Array.from({ length: Math.max(1, Math.min(8, Math.round(pageCount))) }, (_, index) => index + 1);
    const responses = await Promise.allSettled(pages.map((page) =>
      jsonFetch(`https://api.coingecko.com/api/v3/coins/markets?vs_currency=usd&order=volume_desc&per_page=250&page=${page}&sparkline=false&price_change_percentage=1h,24h,7d`, 20000) as Promise<any[]>
    ));
    const rows = responses.flatMap((response) => response.status === 'fulfilled' ? response.value : []);
    const seen = new Set<string>();
    const assets = rows.flatMap((row) => {
      if (!Number.isFinite(row.current_price) || row.current_price <= 0) return [];
      const symbol = cryptoMarketSymbol(row, seen);
      const change = (row.price_change_percentage_24h ?? 0) / 100;
      const previousClose = row.current_price / (1 + change || 1);
      const high = row.high_24h ?? row.current_price;
      const low = row.low_24h ?? row.current_price;
      const fetchedAt = row.last_updated ?? now();
      const meta = metadata(this.name, fetchedAt, freshnessFromAge(fetchedAt, 'live'), 86);
      return [{
        symbol,
        name: row.name,
        assetClass: 'crypto',
        price: row.current_price,
        previousClose,
        changePct: change,
        momentum: clamp(((row.price_change_percentage_1h_in_currency ?? 0) / 100) * 10 + change * 2, -1, 1),
        volatility: Math.max(0.001, Math.min(0.25, Math.abs(high - low) / row.current_price)),
        volume: row.total_volume ?? 0,
        source: this.name,
        marketCap: row.market_cap,
        updatedAt: fetchedAt,
        metadata: meta
      } satisfies MarketAsset];
    });
    return assets.length ? bundle(assets, metadata(this.name, now(), 'delayed', 82)) : null;
  }
}

class DemoProvider implements IMarketProvider {
  id = 'demo';
  name = 'Demo market lab';
  tier = 'demo-fallback' as const;
  capabilities: ProviderCapability[] = ['quotes', 'candles', 'fundamentals', 'news', 'earnings', 'sec-filings', 'etf-exposure', 'crypto', 'macro'];

  async quote(symbol: string, assetClass?: AssetClass) {
    const seed = [...SEEDED_CRYPTO, ...STOCK_UNIVERSE.map((item, index) => ({ symbol: item, name: item, start: 40 + ((index * 37) % 420), vol: 0.006 + (index % 8) * 0.002 }))].find((item) => item.symbol === symbol) ?? {
      symbol,
      name: symbol,
      start: 100 + symbol.length * 7,
      vol: 0.018
    };
    const fetchedAt = now();
    const meta = metadata(this.name, fetchedAt, 'fallback', 48, 'Demo fallback data is active.');
    const move = Math.sin(Date.now() / 3600000 + symbol.length) * seed.vol;
    const price = Math.max(0.01, seed.start * (1 + move));
    const previousClose = seed.start;
    return bundle({
      symbol,
      name: seed.name,
      assetClass: assetClass ?? (symbol.endsWith('-USD') ? 'crypto' : 'stock'),
      price,
      previousClose,
      changePct: previousClose ? (price - previousClose) / previousClose : 0,
      momentum: clamp(move * 12, -1, 1),
      volatility: seed.vol,
      volume: 100000 + Math.abs(Math.round(Math.sin(symbol.length) * 900000)),
      source: this.name,
      updatedAt: fetchedAt,
      metadata: meta
    } satisfies MarketAsset, meta);
  }

  async candles(symbol: string, timeframe: string, assetClass?: AssetClass) {
    const quote = await this.quote(symbol, assetClass);
    const price = quote?.data.price ?? 100;
    const vol = quote?.data.volatility ?? 0.02;
    const candles: HistoricalCandle[] = [];
    for (let index = 179; index >= 0; index -= 1) {
      const timestamp = Date.now() - index * 24 * 60 * 60 * 1000;
      const wave = Math.sin(index * 0.23 + price * 0.01) * vol * price * 1.8;
      const close = Math.max(0.01, price + wave);
      const open = Math.max(0.01, close * (1 + Math.sin(index * 0.41) * vol * 0.55));
      candles.push({
        symbol,
        assetType: quote?.data.assetClass ?? assetClass ?? 'stock',
        timeframe,
        timestamp,
        open,
        high: Math.max(open, close) * (1 + vol * 0.7),
        low: Math.min(open, close) * (1 - vol * 0.7),
        close,
        volume: quote?.data.volume ?? 1000000,
        provider: this.name,
        fetchedAt: now(),
        freshness: 'fallback',
        fallback: true,
        synthetic: true
      });
    }
    return bundle(candles, metadata(this.name, now(), 'fallback', 45, 'Synthetic demo candles.'));
  }
}

export function defaultProviders(): IMarketProvider[] {
  return [new YahooChartProvider(), new CoinGeckoProvider(), new StooqProvider(), new DemoProvider()];
}

export function metadata(provider: string, fetchedAt: string, freshness: DataFreshness, confidence: number, error?: string): MarketDataMetadata {
  const adjusted = confidenceForFreshness(confidence, freshness, error);
  return {
    provider,
    fetchedAt,
    freshness,
    delayed: freshness === 'delayed' || freshness === 'stale' || freshness === 'fallback' || freshness === 'failed',
    fallback: freshness === 'fallback' || freshness === 'failed',
    confidence: adjusted,
    error
  };
}

export function freshnessFromAge(fetchedAt: string, preferred: DataFreshness = 'delayed'): DataFreshness {
  const timestamp = Date.parse(fetchedAt);
  if (!Number.isFinite(timestamp)) return 'failed';
  const age = Date.now() - timestamp;
  if (age < 2 * 60 * 1000 && preferred === 'live') return 'live';
  if (age < 20 * 60 * 1000) return preferred === 'fallback' ? 'fallback' : 'delayed';
  if (age < 24 * 60 * 60 * 1000) return 'stale';
  return 'fallback';
}

function normalizeQuoteBundle(bundle: ProviderBundle<MarketAsset>, provider: IMarketProvider) {
  return {
    ...bundle,
    data: {
      ...bundle.data,
      source: bundle.quality.provider || provider.name,
      metadata: {
        ...bundle.data.metadata,
        provider: bundle.quality.provider,
        fetchedAt: bundle.quality.fetchedAt,
        freshness: bundle.quality.freshness,
        delayed: bundle.quality.delayed,
        fallback: bundle.quality.fallback,
        confidence: bundle.quality.confidence,
        error: bundle.quality.error
      }
    }
  };
}

function bundle<T>(data: T, quality?: MarketDataMetadata): ProviderBundle<T> {
  const meta = quality ?? metadata((data as any).source ?? 'Unknown provider', (data as any).updatedAt ?? now(), 'delayed', 70);
  return {
    data,
    quality: {
      provider: meta.provider,
      fetchedAt: meta.fetchedAt,
      freshness: meta.freshness,
      confidence: meta.confidence,
      delayed: meta.delayed,
      fallback: meta.fallback,
      stale: meta.freshness === 'stale',
      synthetic: meta.provider.toLowerCase().includes('demo') || meta.fallback,
      error: meta.error
    }
  };
}

function withError<T>(bundle: ProviderBundle<T>, error?: string): ProviderBundle<T> {
  if (!error) return bundle;
  return {
    ...bundle,
    quality: { ...bundle.quality, error, confidence: Math.min(bundle.quality.confidence, 45) }
  };
}

function cacheTtlMs(quality: ProviderDataQuality) {
  if (quality.freshness === 'live') return 30_000;
  if (quality.freshness === 'delayed') return 90_000;
  if (quality.freshness === 'stale') return 5 * 60_000;
  return 10 * 60_000;
}

function confidenceForFreshness(base: number, freshness: DataFreshness, error?: string) {
  const penalty = freshness === 'live' ? 0 : freshness === 'delayed' ? 8 : freshness === 'stale' ? 25 : freshness === 'fallback' ? 38 : 55;
  return Math.max(5, Math.min(99, Math.round(base - penalty - (error ? 12 : 0))));
}

async function jsonFetch(url: string, timeoutMs: number): Promise<any> {
  const response = await fetchWithTimeout(url, timeoutMs);
  if (!response.ok) throw new Error(`${response.status} ${response.statusText}`);
  return response.json();
}

async function textFetch(url: string, timeoutMs: number): Promise<string> {
  const response = await fetchWithTimeout(url, timeoutMs);
  if (!response.ok) throw new Error(`${response.status} ${response.statusText}`);
  return response.text();
}

async function fetchWithTimeout(url: string, timeoutMs: number): Promise<Response> {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), timeoutMs);
  try {
    return await fetch(url, {
      signal: controller.signal,
      headers: { 'user-agent': 'AuralithMarkets/0.3 local research provider pipeline' }
    });
  } finally {
    clearTimeout(timeout);
  }
}

function tierRank(tier: ProviderDiagnostics['tier']) {
  return tier === 'primary' ? 0 : tier === 'secondary' ? 1 : tier === 'public-fallback' ? 2 : 3;
}

function normalizeSymbol(symbol: string) {
  return symbol.trim().toUpperCase().replace(/\//g, '-').replace(/\s+/g, '');
}

function cryptoId(symbol: string) {
  const normalized = normalizeSymbol(symbol).replace(/-USD$/, '').toLowerCase();
  const map: Record<string, string> = {
    btc: 'bitcoin',
    eth: 'ethereum',
    sol: 'solana',
    xrp: 'ripple',
    bnb: 'binancecoin',
    doge: 'dogecoin',
    ada: 'cardano',
    avax: 'avalanche-2',
    link: 'chainlink',
    sui: 'sui'
  };
  return map[normalized] ?? normalized;
}

function cryptoMarketSymbol(row: any, seen: Set<string>) {
  const base = `${String(row.symbol ?? '').toUpperCase()}-USD`;
  const unique = seen.has(base) ? `${String(row.symbol ?? '').toUpperCase()}-${normalizeSymbol(String(row.id ?? '')).slice(0, 10)}-USD` : base;
  seen.add(base);
  seen.add(unique);
  return unique;
}

function safeIso(value: string) {
  const timestamp = Date.parse(value);
  return Number.isFinite(timestamp) ? new Date(timestamp).toISOString() : now();
}

function clamp(value: number, min: number, max: number) {
  return Math.min(max, Math.max(min, value));
}
