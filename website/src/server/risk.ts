import type { AccountSummary, MarketAsset, OrderSide, Position, RiskSettings } from './types.js';

export interface RiskDecision {
  allowed: boolean;
  reason: string;
}

export function evaluateOrderRisk(input: {
  settings: RiskSettings;
  account: AccountSummary;
  market: MarketAsset;
  positions: Position[];
  side: OrderSide;
  qty: number;
}): RiskDecision {
  const { settings, account, market, positions, side, qty } = input;
  const notional = qty * market.price;

  if (settings.mode !== 'paper') {
    return { allowed: false, reason: 'Live execution is locked. Only paper executions are allowed in this build.' };
  }
  if (qty <= 0 || !Number.isFinite(qty)) {
    return { allowed: false, reason: 'Quantity must be greater than zero.' };
  }
  if (account.dailyPnlPct <= -settings.dailyLossLimitPct) {
    return { allowed: false, reason: 'Daily loss limit reached.' };
  }
  if (notional > settings.maxTradeNotional) {
    return { allowed: false, reason: 'Order exceeds max trade notional.' };
  }
  if (side === 'buy' && notional > account.cash) {
    return { allowed: false, reason: 'Insufficient paper cash.' };
  }

  const existing = positions.find((position) => position.symbol === market.symbol);
  if (side === 'buy') {
    const projectedValue = (existing?.marketValue ?? 0) + notional;
    if (projectedValue > account.equity * settings.maxPositionPct) {
      return { allowed: false, reason: 'Order would exceed max position size.' };
    }
    if (!existing && positions.length >= settings.maxOpenPositions) {
      return { allowed: false, reason: 'Max open position count reached.' };
    }
  }

  if (side === 'sell') {
    if (!existing || existing.qty < qty) {
      return { allowed: false, reason: 'Cannot sell more than the paper position holds.' };
    }
  }

  return { allowed: true, reason: 'Risk checks passed.' };
}
