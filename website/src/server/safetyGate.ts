import type { RiskSettings } from './types.js';

export type ExecutionIntent = 'paper-order' | 'arbitrage-simulation' | 'live-order';

export interface ExecutionGateInput {
  intent: ExecutionIntent;
  manualConfirmed?: boolean;
}

export interface ExecutionGateResult {
  allowed: boolean;
  reason: string;
  auditType: string;
}

export function evaluateExecutionGate(settings: RiskSettings, input: ExecutionGateInput): ExecutionGateResult {
  if (input.intent === 'live-order') {
    return {
      allowed: false,
      reason: 'Live execution is locked. Auralith currently permits research workflows and paper execution only.',
      auditType: 'execution.live_blocked'
    };
  }

  if (settings.mode !== 'paper' || settings.liveTradingLocked === false) {
    return {
      allowed: false,
      reason: 'Execution safety state is invalid. Paper mode must stay on and live execution must stay locked.',
      auditType: 'execution.safety_state_blocked'
    };
  }

  if (settings.manualConfirmationRequired && !input.manualConfirmed) {
    return {
      allowed: false,
      reason: 'Manual confirmation is required before any paper execution simulation.',
      auditType: 'execution.manual_confirmation_required'
    };
  }

  return {
    allowed: true,
    reason: 'Paper execution allowed after manual confirmation.',
    auditType: 'execution.paper_allowed'
  };
}
