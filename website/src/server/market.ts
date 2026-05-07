import type { MarketAsset } from './types.js';
import { now, Store } from './store.js';
import { fetchRealMarketAssets, fetchStockQuote } from './marketData.js';
import { SEEDED_CRYPTO, STOCK_UNIVERSE } from './universe.js';

const STOCK_SEEDS = STOCK_UNIVERSE.map((symbol, index) => ({
  symbol,
  name: symbol,
  start: 40 + ((index * 37) % 420),
  vol: 0.006 + (index % 8) * 0.002
}));

export class MarketEngine {
  private tickCount = 0;
  private lastRealRefresh = 0;
  private refreshInFlight: Promise<MarketAsset[]> | null = null;
  private readonly refreshMs = Number(process.env.AURALITH_MARKET_REFRESH_MS || process.env.AEGIS_MARKET_REFRESH_MS || 90000);
  private readonly dataMode = (process.env.AURALITH_MARKET_DATA_MODE || process.env.AEGIS_MARKET_DATA_MODE || 'real').toLowerCase();

  constructor(private readonly store: Store) {}

  seed() {
    if (this.store.market().length) return;
    const nowTime = Date.now();
    for (const asset of SEEDED_CRYPTO) {
      this.store.upsertAsset({
        symbol: asset.symbol,
        name: asset.name,
        assetClass: 'crypto',
        price: asset.start,
        previousClose: asset.start * (1 - asset.vol * 0.4),
        momentum: 0,
        volatility: asset.vol,
        volume: 100000 + Math.floor(Math.random() * 900000),
        source: 'simulated',
        updatedAt: now(),
        metadata: simulatedMetadata()
      });
      // Seed historical data
      for (let i = 29; i >= 0; i--) {
        const timestamp = nowTime - (i * 24 * 60 * 60 * 1000);
        const price = asset.start * (1 + (Math.random() - 0.5) * asset.vol * 2);
        const volume = 100000 + Math.floor(Math.random() * 900000);
        this.store.storeHistoricalPrice(asset.symbol, timestamp, price, volume);
      }
    }
    for (const asset of STOCK_SEEDS) {
      this.store.upsertAsset({
        symbol: asset.symbol,
        name: asset.name,
        assetClass: 'stock',
        price: asset.start,
        previousClose: asset.start * (1 - asset.vol * 0.4),
        momentum: 0,
        volatility: asset.vol,
        volume: 100000 + Math.floor(Math.random() * 900000),
        source: 'simulated',
        updatedAt: now(),
        metadata: simulatedMetadata()
      });
      // Seed historical data
      for (let i = 29; i >= 0; i--) {
        const timestamp = nowTime - (i * 24 * 60 * 60 * 1000);
        const price = asset.start * (1 + (Math.random() - 0.5) * asset.vol * 2);
        const volume = 100000 + Math.floor(Math.random() * 900000);
        this.store.storeHistoricalPrice(asset.symbol, timestamp, price, volume);
      }
    }
    this.store.audit('info', 'market.seeded', `Fallback watchlist initialized with ${SEEDED_CRYPTO.length + STOCK_SEEDS.length} assets.`);
  }

  async refreshRealQuotes(force = false): Promise<MarketAsset[]> {
    if (this.dataMode === 'simulated') return this.tick();
    if (!force && Date.now() - this.lastRealRefresh < this.refreshMs) return this.store.market();
    if (this.refreshInFlight) return this.refreshInFlight;

    this.refreshInFlight = fetchRealMarketAssets()
      .then((assets) => {
        if (!assets.length) throw new Error('No real quote assets returned.');
        for (const asset of assets) this.store.upsertAsset(asset);
        this.lastRealRefresh = Date.now();
        this.store.audit('info', 'market.real_refresh', `Real market data refreshed for ${assets.length} assets.`);
        return this.store.market();
      })
      .catch((error) => {
        this.store.audit('warning', 'market.real_refresh_failed', `Real market refresh failed: ${String(error)}`);
        return this.tick();
      })
      .finally(() => {
        this.refreshInFlight = null;
      });

    return this.refreshInFlight;
  }

  async addStockSymbol(symbol: string): Promise<MarketAsset> {
    const quote = await fetchStockQuote(symbol);
    if (!quote) throw new Error(`No quote returned for ${symbol}.`);
    this.store.upsertAsset(quote);
    this.store.audit('info', 'market.symbol_added', `${quote.symbol} added to the market board from ${quote.source}.`, {
      symbol: quote.symbol,
      source: quote.source
    });
    return quote;
  }

  tick(): MarketAsset[] {
    this.tickCount += 1;
    const current = this.store.market();
    for (const asset of current) {
      if (asset.source !== 'simulated') continue;
      const config = [...SEEDED_CRYPTO, ...STOCK_SEEDS].find((item) => item.symbol === asset.symbol);
      const vol = config?.vol ?? asset.volatility;
      const drift = Math.sin((this.tickCount + asset.symbol.length) / 9) * vol * 0.35;
      const shock = (Math.random() - 0.5) * vol;
      const move = drift + shock;
      const price = Math.max(0.01, asset.price * (1 + move));
      const momentum = clamp(asset.momentum * 0.72 + move * 12, -1, 1);
      const volatility = clamp(asset.volatility * 0.85 + Math.abs(move) * 2.5, 0.001, 0.08);
      const volume = Math.max(1000, asset.volume * (0.94 + Math.random() * 0.14));
      const updatedAt = now();
      this.store.upsertAsset({ ...asset, price, momentum, volatility, volume, source: 'simulated', updatedAt, metadata: simulatedMetadata(updatedAt) });
    }
    return this.store.market();
  }
}

function simulatedMetadata(updatedAt = now()) {
  return {
    provider: 'simulated',
    fetchedAt: updatedAt,
    freshness: 'fallback' as const,
    delayed: true,
    fallback: true,
    confidence: 55
  };
}

function clamp(value: number, min: number, max: number) {
  return Math.min(max, Math.max(min, value));
}
