import crypto from 'node:crypto';
import type { OrderSide } from './types.js';
import { Store } from './store.js';
import { evaluateExecutionGate } from './safetyGate.js';

// Live execution is intentionally blocked unless a future, audited adapter explicitly unlocks it.
export class LiveTrader {
  constructor(private readonly store: Store) {}

  async placeLiveOrder(symbol: string, side: OrderSide, qty: number): Promise<any> {
    const orderId = crypto.randomUUID();
    const gate = evaluateExecutionGate(this.store.settings(), { intent: 'live-order', manualConfirmed: false });
    this.store.audit('warning', gate.auditType, gate.reason, { symbol, side, qty, orderId });
    return { orderId, status: 'rejected', reason: gate.reason };
  }

  async getLiveBalance(): Promise<any> {
    // Placeholder: fetch real balance
    return { cash: 10000, positions: [] };
  }
}
