import crypto from 'node:crypto';
import type { AccountSummary, MarketAsset, OrderSide, Position } from './types.js';
import { now, Store } from './store.js';
import { evaluateOrderRisk } from './risk.js';
import { evaluateExecutionGate } from './safetyGate.js';
import { buildSignals, detectMarketRegime, evolveStrategies } from './strategy.js';

export class Trader {
  constructor(private readonly store: Store) {}

  accountSummary(): AccountSummary {
    this.refreshPositionPrices();
    const account = this.store.account();
    const positions = this.store.positions();
    const exposure = positions.reduce((sum, position) => sum + position.marketValue, 0);
    const unrealizedPnl = positions.reduce((sum, position) => sum + position.unrealizedPnl, 0);
    const equity = account.cash + exposure;
    const dailyPnl = equity - account.day_start_equity;
    const maxDrawdownPct = account.equity_high ? Math.min(0, (equity - account.equity_high) / account.equity_high) : 0;
    if (equity > account.equity_high) {
      this.store.db.prepare('update account set equity_high = ?, updated_at = ? where id = ?').run(equity, now(), 'paper');
    }
    return {
      cash: account.cash,
      equity,
      startingEquity: account.starting_equity,
      realizedPnl: account.realized_pnl,
      unrealizedPnl,
      exposure,
      dailyPnl,
      dailyPnlPct: account.day_start_equity ? dailyPnl / account.day_start_equity : 0,
      maxDrawdownPct
    };
  }

  async state(options: { signalUniverseLimit?: number; includeSymbols?: string[] } = {}) {
    const market = this.store.market();
    const positions = this.store.positions();
    const account = this.accountSummary();
    const settings = this.store.settings();
    const regime = detectMarketRegime(market);
    const signalMarket = options.signalUniverseLimit
      ? prioritizeSignalUniverse(market, positions, options.signalUniverseLimit, options.includeSymbols ?? [])
      : market;
    return {
      account,
      settings,
      market,
      positions,
      orders: this.store.orders(),
      signals: await buildSignals(signalMarket, positions, account, settings, new Map()),
      audit: this.store.auditEvents(),
      regime,
      strategyPerformance: this.store.getStrategyPerformance(),
      generatedAt: now()
    };
  }

  async strategyCycle() {
    const before = await this.state();
    if (!before.settings.autopilot) {
      this.store.audit('info', 'strategy.review', 'Strategy cycle reviewed signals. Signal review automation is off.');
      return before;
    }

    // Prune bad strategies
    const performance = this.store.getStrategyPerformance();
    const badStrategies = performance.filter(p => p.total_trades >= 10 && (p.total_pnl < -100 || p.win_rate < 0.4)).map(p => p.strategy);
    if (badStrategies.length > 0) {
      const updatedDisabled = [...new Set([...before.settings.disabledStrategies, ...badStrategies])];
      this.store.updateSettings({ disabledStrategies: updatedDisabled });
      this.store.audit('warning', 'strategy.prune', `Disabled bad strategies: ${badStrategies.join(', ')}`);
    }

    // Evolve strategies
    const evolution = evolveStrategies(performance, before.settings);
    if (Object.keys(evolution).length > 0) {
      this.store.updateSettings(evolution);
      this.store.audit('info', 'strategy.evolve', `Evolved settings: ${JSON.stringify(evolution)}`);
    }

    if (before.settings.manualConfirmationRequired) {
      this.store.audit('warning', 'strategy.paper_execution_blocked', 'Strategy cycle did not place paper executions because manual confirmation is required.');
      return before;
    }

    const actionable = before.signals
      .filter((signal) => signal.side !== 'hold' && signal.confidence >= before.settings.minConfidence)
      .filter((signal) => !before.settings.disabledStrategies.includes(signal.reason.split('.')[0])) // Simple strategy extraction
      .slice(0, 3);
    for (const signal of actionable) {
      const market = before.market.find((asset) => asset.symbol === signal.symbol);
      if (!market) continue;
      const notional = Math.min(before.settings.maxTradeNotional, before.account.equity * before.settings.maxPositionPct * 0.5);
      const qty = signal.side === 'buy' ? notional / market.price : this.store.positions().find((item) => item.symbol === signal.symbol)?.qty ?? 0;
      await this.placeOrder(signal.symbol, signal.side as OrderSide, qty, signal.reason, false);
    }
    return this.state();
  }

  async placeOrder(symbol: string, side: OrderSide, qty: number, signal = 'Manual paper execution', manualConfirmed = false) {
    const market = this.store.market().find((asset) => asset.symbol === symbol);
    if (!market) throw new Error(`Unknown symbol: ${symbol}`);
    const settings = this.store.settings();
    const account = this.accountSummary();
    const positions = this.store.positions();
    const id = crypto.randomUUID();
    const notional = qty * market.price;
    const gate = evaluateExecutionGate(settings, { intent: 'paper-order', manualConfirmed });

    if (!gate.allowed) {
      this.recordOrder(id, symbol, side, qty, 'rejected', market.price, notional, gate.reason, signal);
      this.store.audit('warning', gate.auditType, `${side.toUpperCase()} ${symbol} blocked: ${gate.reason}`, { symbol, side, qty });
      return this.state();
    }

    const risk = evaluateOrderRisk({ settings, account, market, positions, side, qty });

    if (!risk.allowed) {
      this.recordOrder(id, symbol, side, qty, 'rejected', market.price, notional, risk.reason, signal);
      this.store.audit('warning', 'order.rejected', `${side.toUpperCase()} ${symbol} rejected: ${risk.reason}`, { symbol, side, qty });
      return this.state();
    }

    if (side === 'buy') this.fillBuy(symbol, market, qty);
    if (side === 'sell') {
      const existing = this.store.positions().find((position) => position.symbol === symbol);
      const realized = existing ? (market.price - existing.avgPrice) * qty : 0;
      this.fillSell(symbol, market, qty);
      this.store.updateStrategyPerformance(signal, realized, realized > 0);
    }
    this.recordOrder(id, symbol, side, qty, 'filled', market.price, notional, risk.reason, signal);
    this.store.audit('info', 'order.filled', `${side.toUpperCase()} ${symbol} filled in Auralith paper mode.`, { symbol, side, qty, price: market.price });
    return this.state();
  }

  private fillBuy(symbol: string, market: MarketAsset, qty: number) {
    const existing = this.store.positions().find((position) => position.symbol === symbol);
    const account = this.store.account();
    const notional = qty * market.price;
    this.store.db.prepare('update account set cash = ?, updated_at = ? where id = ?').run(account.cash - notional, now(), 'paper');

    if (!existing) {
      this.store.db.prepare('insert into positions (symbol, asset_class, qty, avg_price, last_price, entry_time) values (?, ?, ?, ?, ?, ?)').run(
        symbol,
        market.assetClass,
        qty,
        market.price,
        market.price,
        now()
      );
      return;
    }

    const nextQty = existing.qty + qty;
    const nextAvg = (existing.avgPrice * existing.qty + market.price * qty) / nextQty;
    this.store.db.prepare('update positions set qty = ?, avg_price = ?, last_price = ? where symbol = ?').run(nextQty, nextAvg, market.price, symbol);
  }

  private fillSell(symbol: string, market: MarketAsset, qty: number) {
    const existing = this.store.positions().find((position) => position.symbol === symbol);
    if (!existing) return;
    const account = this.store.account();
    const notional = qty * market.price;
    const realized = (market.price - existing.avgPrice) * qty;
    this.store.db.prepare('update account set cash = ?, realized_pnl = ?, updated_at = ? where id = ?').run(
      account.cash + notional,
      account.realized_pnl + realized,
      now(),
      'paper'
    );
    const nextQty = existing.qty - qty;
    if (nextQty <= 0.0000001) {
      this.store.db.prepare('delete from positions where symbol = ?').run(symbol);
    } else {
      this.store.db.prepare('update positions set qty = ?, last_price = ? where symbol = ?').run(nextQty, market.price, symbol);
    }
  }

  private recordOrder(
    id: string,
    symbol: string,
    side: OrderSide,
    qty: number,
    status: 'filled' | 'rejected',
    price: number,
    notional: number,
    reason: string,
    signal: string
  ) {
    this.store.db.prepare(`
      insert into orders (id, created_at, symbol, side, qty, status, mode, price, notional, reason, signal)
      values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `).run(id, now(), symbol, side, qty, status, 'paper', price, notional, reason, signal);
  }

  private refreshPositionPrices() {
    const market = this.store.market();
    for (const position of this.store.positions()) {
      const asset = market.find((item) => item.symbol === position.symbol);
      if (asset) this.store.db.prepare('update positions set last_price = ? where symbol = ?').run(asset.price, position.symbol);
    }
  }
}

function prioritizeSignalUniverse(market: MarketAsset[], positions: Position[], limit: number, includeSymbols: string[]) {
  const bySymbol = new Map(market.map((asset) => [asset.symbol, asset]));
  const important = new Set<string>([
    ...positions.map((position) => position.symbol),
    ...includeSymbols.map((symbol) => symbol.toUpperCase()),
    'SPY',
    'QQQ',
    'DIA',
    'VIX',
    'AAPL',
    'MSFT',
    'NVDA',
    'AMZN',
    'GOOGL',
    'META',
    'TSLA'
  ]);
  const selected = [...important]
    .map((symbol) => bySymbol.get(symbol))
    .filter((asset): asset is MarketAsset => Boolean(asset));
  const ranked = [...market]
    .filter((asset) => !important.has(asset.symbol))
    .sort((a, b) => signalPriorityScore(b) - signalPriorityScore(a));
  const merged = [...selected, ...ranked].filter((asset, index, assets) => assets.findIndex((item) => item.symbol === asset.symbol) === index);
  return merged.slice(0, Math.max(important.size, limit));
}

function signalPriorityScore(asset: MarketAsset) {
  const confidence = (asset.metadata?.confidence ?? 50) / 100;
  const freshnessBoost = asset.metadata?.freshness === 'live' ? 1.2 : asset.metadata?.freshness === 'delayed' ? 0.8 : 0.2;
  const stockBias = asset.assetClass === 'stock' ? 0.7 : 0;
  return Math.abs(asset.changePct) * 80 + asset.volatility * 20 + confidence + freshnessBoost + stockBias;
}
