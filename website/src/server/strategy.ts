import type { AccountSummary, MarketAsset, OrderSide, Position, RiskSettings, Signal } from './types.js';
import { WalletAnalyzer } from './walletAnalyzer.js';
import { SentimentAnalyzer } from './sentiment.js';
// import { PricePredictor } from './predictor.js';

const walletAnalyzer = new WalletAnalyzer();
const sentimentAnalyzer = new SentimentAnalyzer();
// const pricePredictor = new PricePredictor();

export type MarketRegime = 'trending' | 'sideways' | 'volatile' | 'unknown';

export function detectMarketRegime(market: MarketAsset[]): MarketRegime {
  if (market.length === 0) return 'unknown';

  // Calculate average volatility and trend strength
  const avgVolatility = market.reduce((sum, asset) => sum + asset.volatility, 0) / market.length;
  const avgMomentum = Math.abs(market.reduce((sum, asset) => sum + asset.momentum, 0) / market.length);
  const avgChange = Math.abs(market.reduce((sum, asset) => sum + asset.changePct, 0) / market.length);

  // Regime thresholds (these can be tuned)
  if (avgVolatility > 0.05 && avgMomentum > 0.3) return 'volatile';
  if (avgMomentum > 0.2) return 'trending';
  if (avgChange < 0.02) return 'sideways';

  return 'unknown';
}

export async function buildSignals(market: MarketAsset[], positions: Position[], account: AccountSummary, settings: RiskSettings, historicalData?: Map<string, any[]>): Promise<Signal[]> {
  const regime = detectMarketRegime(market);
  const sentimentData = new Map(); // await sentimentAnalyzer.getBulkSentiment(market.map(m => m.symbol));
  const predictions = new Map(); // historicalData ? await pricePredictor.getPredictions(market.map(m => m.symbol), historicalData) : new Map();

  return market
    .map((asset) => {
      const position = positions.find((item) => item.symbol === asset.symbol);

      const history = historicalData?.get(asset.symbol) ?? [];
      const prices = priceSeries(asset, history);
      const components = momentumComponents(asset, prices, regime);
      const momentumScore = Math.round(
        components.trend * 0.25 +
        components.volume * 0.2 +
        components.oscillator * 0.2 +
        components.breakout * 0.15 +
        components.sector * 0.1 +
        components.regime * 0.1
      );
      const normalizedMomentum = clamp(momentumScore / 100, 0, 1);
      const walletSignal = walletAnalyzer.getWalletSignal(asset);
      const sentiment = sentimentData.get(asset.symbol);
      const sentimentSignal = 0; // sentiment ? sentiment.score * sentiment.confidence : 0;
      // const prediction = predictions.get(asset.symbol);
      // const predictionSignal = prediction ? (prediction.direction === 'up' ? 0.2 : prediction.direction === 'down' ? -0.2 : 0) * prediction.confidence : 0;
      const volatilityPenalty = Math.min(0.24, asset.volatility * 3);
      const dataConfidence = (asset.metadata?.confidence ?? (asset.source === 'simulated' ? 55 : 70)) / 100;

      let confidenceBase = normalizedMomentum * 0.76 + dataConfidence * 0.14 + walletSignal * 0.07 + sentimentSignal * 0.03 - volatilityPenalty * 0.25;

      if (regime === 'trending') {
        confidenceBase += 0.04;
      } else if (regime === 'sideways') {
        confidenceBase -= 0.04;
      } else if (regime === 'volatile') {
        confidenceBase -= 0.08;
      }
      if (asset.assetClass === 'crypto') {
        confidenceBase -= 0.12;
      }

      const confidenceScore = clamp(confidenceBase, 0, 1);
      const score = momentumScore;
      let side: OrderSide | 'hold' = 'hold';
      const paperEntryThreshold = asset.assetClass === 'crypto' ? 88 : 75;

      if (!position && normalizedMomentum >= settings.minConfidence && momentumScore >= paperEntryThreshold) {
        if (regime !== 'volatile' || momentumScore >= 85) {
          side = 'buy';
        }
      }

      if (position) {
        const pnlPct = position.unrealizedPnlPct;
        const timeHeld = (Date.now() - position.entryTime) / (1000 * 60 * 60);

        if (pnlPct < -0.035 || // Stop loss
            pnlPct > 0.06 || // Take profit
            (regime === 'volatile' && pnlPct < -0.02) || // Tighter stop in volatile markets
            (timeHeld > 24 && pnlPct < 0.01) || // Time-based exit
            momentumScore < 40) { // Momentum failure
          side = 'sell';
        }
      }

      const confidence = side === 'hold' ? confidenceScore : side === 'buy' ? confidenceScore : clamp(1 - normalizedMomentum + Math.abs(position?.unrealizedPnlPct ?? 0), 0, 1);
      const plan = paperSetupPlan(asset, position, side, momentumScore, components, regime, account, settings, prices);

      return {
        symbol: asset.symbol,
        assetClass: asset.assetClass,
        currentPrice: asset.price,
        side,
        confidence,
        score,
        setupType: plan.setupType,
        entry: plan.entry,
        stop: plan.stop,
        target1: plan.target1,
        target2: plan.target2,
        target: plan.target,
        trailingStop: plan.trailingStop,
        resistance: plan.resistance,
        atr: plan.atr,
        expectedSellZone: plan.expectedSellZone,
        exitPlan: plan.exitPlan,
        invalidation: plan.invalidation,
        riskReward: plan.riskReward,
        riskScore: plan.riskScore,
        momentumScore,
        manualConfirmationRequired: true,
        paperOnly: true,
        components,
        reason: explainSignal(asset, position, side, momentumScore, regime, plan.setupType, components, plan),
        risk: plan.risk
      };
    })
    .sort((a, b) => b.confidence - a.confidence);
}

function explainSignal(asset: MarketAsset, position: Position | undefined, side: OrderSide | 'hold', momentumScore: number, regime: MarketRegime, setupType: string, components: Signal['components'], plan: ReturnType<typeof paperSetupPlan>) {
  const regimeText = regime !== 'unknown' ? `${regime} market regime` : 'unclear market regime';
  const strengths = Object.entries(components)
    .filter(([, value]) => value >= 60)
    .sort((a, b) => b[1] - a[1])
    .slice(0, 2)
    .map(([key, value]) => `${componentLabel(key)} ${Math.round(value)}/100`);
  const constraints = Object.entries(components)
    .filter(([, value]) => value < 55)
    .sort((a, b) => a[1] - b[1])
    .slice(0, 2)
    .map(([key, value]) => `${componentLabel(key)} ${Math.round(value)}/100`);
  const dataConfidence = asset.metadata?.confidence ?? (asset.source === 'simulated' ? 55 : 70);
  const sourceText = `${asset.metadata?.freshness ?? 'fallback'} data from ${asset.metadata?.provider ?? asset.source} at ${dataConfidence}% confidence`;
  const constraintText = constraints.length ? `Constraints: ${constraints.join(', ')}.` : 'No single component is dominating the weakness profile.';
  const strengthText = strengths.length ? strengths.join(', ') : 'confirmation remains limited';

  if (side === 'buy') {
    return `${setupType}. Momentum remains constructive at ${momentumScore}/100 because ${strengthText}. ${constraintText} ${sourceText}. Paper entry is only considered above ${money(plan.entry)}, with invalidation near ${money(plan.stop)} and manual confirmation required.`;
  }
  if (side === 'sell' && position) {
    return `${setupType}. Paper exit review is triggered at ${(position.unrealizedPnlPct * 100).toFixed(2)}% paper P/L with ${regimeText}. ${constraintText} Manual confirmation remains required before changing the paper position.`;
  }
  return `${setupType}. Momentum score ${momentumScore}/100 is not enough for a confirmed paper plan under the current ${regimeText}. ${constraintText} Watch for trend, participation, and risk/reward to improve together before acting.`;
}

export async function runBacktest(historicalData: { symbol: string; timestamp: number; price: number; volume: number }[], initialCapital: number = 10000): Promise<any> {
  const symbols = [...new Set(historicalData.map(d => d.symbol))];
  const dataBySymbol = symbols.reduce((acc, sym) => {
    acc[sym] = historicalData.filter(d => d.symbol === sym).sort((a, b) => a.timestamp - b.timestamp);
    return acc;
  }, {} as Record<string, typeof historicalData>);

  let capital = initialCapital;
  const positions: Map<string, { qty: number; entryPrice: number }> = new Map();
  const trades: any[] = [];

  // Simulate over time
  const allTimestamps = [...new Set(historicalData.map(d => d.timestamp))].sort((a, b) => a - b);

  for (const timestamp of allTimestamps) {
    const currentData = symbols.map(sym => {
      const data = dataBySymbol[sym].find(d => d.timestamp === timestamp);
      if (!data) return null;
      return {
        symbol: sym,
        name: sym,
        assetClass: 'crypto' as const,
        price: data.price,
        previousClose: data.price * 0.99,
        changePct: 0,
        momentum: 0,
        volatility: 0.05,
        volume: data.volume,
        source: 'historical',
        updatedAt: new Date(data.timestamp).toISOString()
      };
    }).filter(Boolean) as MarketAsset[];

    if (currentData.length === 0) continue;

    // Build signals for current data
    const signals = await buildSignals(currentData, [], { equity: capital, dailyPnlPct: 0 } as any, { minConfidence: 0.5 } as any, new Map(Object.entries(dataBySymbol)));

    for (const signal of signals) {
      const asset = currentData.find(a => a.symbol === signal.symbol);
      if (!asset) continue;

      if (signal.side === 'buy' && !positions.has(asset.symbol)) {
        const qty = Math.floor((capital * 0.1) / asset.price);
        if (qty > 0) {
          positions.set(asset.symbol, { qty, entryPrice: asset.price });
          capital -= qty * asset.price;
          trades.push({ type: 'buy', symbol: asset.symbol, price: asset.price, qty, timestamp, capital });
        }
      } else if (signal.side === 'sell' && positions.has(asset.symbol)) {
        const position = positions.get(asset.symbol)!;
        capital += position.qty * asset.price;
        const pnl = (asset.price - position.entryPrice) * position.qty;
        trades.push({ type: 'sell', symbol: asset.symbol, price: asset.price, qty: position.qty, pnl, timestamp, capital });
        positions.delete(asset.symbol);
      }
    }
  }

  const finalEquity = capital + Array.from(positions.entries()).reduce((sum, [sym, pos]) => sum + pos.qty * (dataBySymbol[sym]?.slice(-1)[0]?.price || 0), 0);
  const totalReturn = ((finalEquity - initialCapital) / initialCapital) * 100;

  return {
    initialCapital,
    finalEquity,
    totalReturn,
    trades,
    winRate: trades.filter(t => t.type === 'sell' && t.pnl > 0).length / trades.filter(t => t.type === 'sell').length
  };
}

function normalize(value: number, min: number, max: number) {
  return (value - min) / (max - min);
}

function priceSeries(asset: MarketAsset, rows: any[]) {
  const values = rows
    .map((row) => Number(row.price))
    .filter((value) => Number.isFinite(value) && value > 0);
  if (asset.previousClose > 0) values.push(asset.previousClose);
  if (asset.price > 0) values.push(asset.price);
  return values.length > 0 ? values : [1];
}

function averageLast(values: number[], count: number, offset = 0) {
  const end = Math.max(0, values.length - offset);
  if (end === 0) return values[0] ?? 0;
  const start = Math.max(0, end - count);
  const slice = values.slice(start, end);
  return slice.reduce((sum, value) => sum + value, 0) / Math.max(slice.length, 1);
}

function rateOfChange(values: number[], lookback: number) {
  if (values.length < 2) return 0;
  const index = Math.max(0, values.length - 1 - lookback);
  const base = values[index];
  return base > 0 ? ((values[values.length - 1] - base) / base) * 100 : 0;
}

function rsi14(values: number[]) {
  if (values.length < 2) return 50;
  const periods = Math.min(14, values.length - 1);
  let gains = 0;
  let losses = 0;
  for (let index = values.length - periods; index < values.length; index += 1) {
    const delta = values[index] - values[index - 1];
    if (delta >= 0) gains += delta;
    else losses -= delta;
  }
  const avgGain = gains / periods;
  const avgLoss = losses / periods;
  if (avgLoss <= 0.000001) return avgGain > 0 ? 99 : 50;
  const rs = avgGain / avgLoss;
  return 100 - 100 / (1 + rs);
}

function macdHistogram(values: number[]) {
  let ema12 = values[0] ?? 0;
  let ema26 = values[0] ?? 0;
  let signal = 0;
  let seeded = false;
  let macd = 0;
  for (const value of values) {
    ema12 += (2 / 13) * (value - ema12);
    ema26 += (2 / 27) * (value - ema26);
    macd = ema12 - ema26;
    if (!seeded) {
      signal = macd;
      seeded = true;
    } else {
      signal += (2 / 10) * (macd - signal);
    }
  }
  return macd - signal;
}

function averageTrueRangeProxy(asset: MarketAsset, values: number[]) {
  if (values.length < 2) return asset.price * clamp(asset.volatility, 0.008, 0.08);
  const sample = values.slice(Math.max(1, values.length - 14));
  const closeToClose = sample.reduce((sum, value, index) => {
    const previous = values[Math.max(0, values.length - sample.length + index - 1)];
    return sum + Math.abs(value - previous);
  }, 0) / Math.max(sample.length, 1);
  const volatilityRange = asset.price * clamp(asset.volatility, 0.006, 0.08);
  return Math.max(asset.price * 0.003, closeToClose, volatilityRange);
}

function momentumComponents(asset: MarketAsset, prices: number[], regime: MarketRegime) {
  const price = asset.price;
  const sma20 = averageLast(prices, 20);
  const sma50 = averageLast(prices, 50);
  const priorSma50 = averageLast(prices, 50, Math.min(5, Math.max(0, prices.length - 1)));
  const roc5 = rateOfChange(prices, 5);
  const roc20 = rateOfChange(prices, 20);
  const rsi = rsi14(prices);
  const macd = macdHistogram(prices);
  const recentHigh = prices.length > 1 ? Math.max(...prices.slice(Math.max(0, prices.length - 31), -1)) : price;

  if (isStableLike(asset)) {
    return {
      trend: 35,
      volume: 45,
      oscillator: 40,
      breakout: 25,
      sector: 35,
      regime: 45
    };
  }
  if (isSpeculativeMicroCrypto(asset)) {
    return {
      trend: 38,
      volume: 42,
      oscillator: 38,
      breakout: 24,
      sector: 30,
      regime: 35
    };
  }

  let trend = 42;
  trend += price > sma20 ? 16 : -10;
  trend += price > sma50 ? 15 : -8;
  if (sma20 > sma50) trend += 12;
  if (sma50 >= priorSma50) trend += 8;
  if (roc5 > 0) trend += 5;
  if (roc20 > 0) trend += 5;

  let volume = 38;
  const logVolume = normalize(Math.log(asset.volume + 1), 0, 20);
  volume += Math.round(clamp(logVolume, 0, 1) * 36);
  if (asset.changePct > 0) volume += 9;
  if (asset.changePct < 0) volume -= 8;

  let oscillator = 48;
  if (rsi >= 50 && rsi <= 70) oscillator += 22;
  else if (rsi > 70 && rsi <= 75) oscillator += 8;
  else if (rsi > 75) oscillator -= 14;
  else if (rsi < 40) oscillator -= 18;
  oscillator += macd > 0 ? 16 : -10;
  if (roc5 > 0 && roc20 > 0) oscillator += 6;

  let breakout = 42;
  if (recentHigh > 0 && price > recentHigh * 1.001 && asset.volume > 750000) breakout += 34;
  else if (recentHigh > 0 && price >= recentHigh * 0.985) breakout += 14;
  if (volume >= 70) breakout += 8;
  if (asset.changePct > 0.01) breakout += 6;

  let sector = asset.assetClass === 'stock' ? 56 : 52;
  if (asset.changePct > 0) sector += 8;
  if (asset.changePct < -0.01) sector -= 10;

  let regimeScore = regime === 'trending' ? 70 : regime === 'volatile' ? 38 : regime === 'sideways' ? 47 : 58;
  if (asset.changePct > 0.0075) regimeScore += 8;
  if (asset.changePct < -0.0075) regimeScore -= 10;
  if (asset.volatility > 0.055) regimeScore -= 8;

  return {
    trend: Math.round(clamp(trend, 0, 100)),
    volume: Math.round(clamp(volume, 0, 100)),
    oscillator: Math.round(clamp(oscillator, 0, 100)),
    breakout: Math.round(clamp(breakout, 0, 100)),
    sector: Math.round(clamp(sector, 0, 100)),
    regime: Math.round(clamp(regimeScore, 0, 100))
  };
}

function paperSetupPlan(asset: MarketAsset, position: Position | undefined, side: OrderSide | 'hold', momentumScore: number, components: Signal['components'], regime: MarketRegime, account: AccountSummary, settings: RiskSettings, prices: number[]) {
  const entry = side === 'buy' ? asset.price * 1.002 : asset.price;
  const stopDistance = clamp(asset.volatility * 1.8, 0.035, 0.12);
  const stop = entry * (1 - stopDistance);
  const riskPerUnit = Math.max(entry - stop, Math.max(entry * 0.0001, 0.00000001));
  const atr = averageTrueRangeProxy(asset, prices);
  const recentHigh = prices.length > 1 ? Math.max(...prices.slice(Math.max(0, prices.length - 31), -1)) : asset.price;
  const resistance = recentHigh > entry ? recentHigh : null;
  const target2r = entry + riskPerUnit * 2;
  const atrModerate = entry + atr * 2;
  const atrAggressive = entry + atr * 3;
  const target1 = Math.max(target2r, atrModerate);
  const continuationStep = Math.max(Math.min(atr, riskPerUnit * 0.75), entry * 0.003);
  const continuationTarget = entry + Math.max(riskPerUnit * 3, atr * 3);
  const uncappedTarget2 = resistance && resistance >= target1 * 0.985
    ? Math.max(target1, resistance)
    : Math.max(target1 + continuationStep, continuationTarget, atrAggressive);
  const maxReasonableTarget = entry + Math.max(riskPerUnit * 3.5, atr * 4);
  const target2 = Math.min(uncappedTarget2, maxReasonableTarget);
  const target = target2;
  const trailingStop = Math.max(stop, entry - atr * 2);
  const riskReward = (target1 - entry) / riskPerUnit;
  const dataPenalty = 100 - (asset.metadata?.confidence ?? (asset.source === 'simulated' ? 55 : 70));
  const riskScore = Math.round(clamp(asset.volatility * 900 + dataPenalty * 0.35 + (regime === 'volatile' ? 18 : 0) + (account.dailyPnlPct < -settings.dailyLossLimitPct * 0.7 ? 18 : 0), 0, 100));
  const risk = account.dailyPnlPct < -settings.dailyLossLimitPct * 0.7
    ? 'High: drawdown guard near limit'
    : riskScore >= 70
      ? 'High: volatility or data-quality risk elevated'
      : riskScore >= 50
        ? 'Medium: review sizing and stop distance'
        : 'Normal';
  const setupType = side === 'buy'
    ? momentumScore >= 90 ? 'High-conviction paper setup' : 'Paper Long Setup Detected'
    : side === 'sell'
      ? position && position.unrealizedPnlPct < -0.035
        ? 'Stop-loss paper exit review'
        : position && position.unrealizedPnlPct > 0.06
          ? 'Take-profit paper exit review'
          : 'Momentum failure paper exit review'
      : momentumScore >= 60
        ? 'Paper Watch Setup'
        : momentumScore < 40
          ? 'Momentum Failure Review'
          : 'No Paper Setup';
  const invalidation = side === 'buy'
    ? `Invalidated if price closes below ${money(stop)} or momentum score falls below 50.`
    : side === 'sell'
      ? 'Paper exit review remains invalid until the stop, target, or momentum-failure condition is manually confirmed.'
      : 'No setup is active until trend, oscillator, participation, and regime scores improve together.';
  const expectedSellZone = `${money(target1)} to ${money(target2)}`;
  const exitPlan = `Review 50% near ${money(target1)}, review 25% near ${money(target2)}, and trail the remaining 25% at 2x ATR below the highest close.`;

  return { entry, stop, target1, target2, target, trailingStop, resistance, atr, expectedSellZone, exitPlan, riskReward, riskScore, risk, setupType, invalidation, components };
}

function isStableLike(asset: MarketAsset) {
  return asset.assetClass === 'crypto' && /(?:USDC|USDT|DAI|BUSD)/i.test(asset.symbol) && asset.price > 0.8 && asset.price < 1.2;
}

function isSpeculativeMicroCrypto(asset: MarketAsset) {
  return asset.assetClass === 'crypto' && asset.price > 0 && asset.price < 0.05;
}

function money(value: number) {
  return `$${value.toFixed(value >= 100 ? 2 : 4)}`;
}

function componentLabel(value: string) {
  return value.replace(/([A-Z])/g, ' $1').replace(/^./, (letter) => letter.toUpperCase());
}

export function evolveStrategies(performance: any[], settings: RiskSettings): Partial<RiskSettings> {
  // Simple evolution: boost weights for good strategies, reduce for bad
  const updates: Partial<RiskSettings> = {};

  const momentumPerf = performance.find(p => p.strategy.includes('Momentum'));
  const volumePerf = performance.find(p => p.strategy.includes('volume'));

  if (momentumPerf && momentumPerf.win_rate > 0.6) {
    updates.minConfidence = Math.max(0.5, settings.minConfidence - 0.05); // Lower threshold for good momentum
  } else if (momentumPerf && momentumPerf.win_rate < 0.4) {
    updates.minConfidence = Math.min(0.9, settings.minConfidence + 0.05); // Raise threshold
  }

  return updates;
}

function clamp(value: number, min: number, max: number) {
  return Math.min(max, Math.max(min, value));
}
