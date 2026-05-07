# Auralith Momentum Setup Engine

Auralith treats momentum as a research setup, not as an instruction to trade. The engine measures whether price, participation, oscillators, breakout behavior, sector context, and market regime are moving in the same direction, then drafts a paper plan that still requires manual confirmation.

It does not claim to predict future prices. It estimates probability-based continuation zones:

```text
If this trend continues, where is a reasonable exit zone before risk becomes too high?
```

## Momentum Score

The momentum score is a 0-100 weighted model:

- Trend: 25%
- Volume confirmation: 20%
- RSI and MACD: 20%
- Breakout strength: 15%
- Sector context: 10%
- Market regime: 10%

Score bands:

- 0-39: Weak momentum
- 40-59: Neutral research
- 60-74: Paper watch setup
- 75-89: Paper long setup detected
- 90-100: High-conviction paper setup, still requiring confirmation

## Setup Contract

Every generated setup carries:

- setup type
- entry idea
- invalidation rule
- stop
- target 1
- target 2
- trailing stop
- ATR estimate
- resistance estimate
- expected sell zone
- risk/reward
- risk score
- confidence
- source and freshness context
- paper-only flag
- manual-confirmation flag

The intended workflow is:

```text
detect setup -> explain why -> calculate risk -> draft paper plan -> require manual confirmation -> log execution attempts
```

Auralith must not imply guaranteed outcomes, autonomous live trading, or financial advice.

## Expected Sell Zones

Auralith uses multiple target methods and blends them into a practical paper exit plan:

- Risk/reward target: target 1 must respect at least 2R when the setup is valid.
- Resistance target: prior highs can become logical sell zones where momentum may stall.
- ATR target: expected movement is capped by normal volatility so targets do not become fantasy prices.
- Trailing stop: remaining paper exposure is protected with a 2x ATR trail below the highest close.

The default partial-exit model is:

```text
Target 1: review 50%
Target 2: review 25%
Trailing stop: keep 25% while momentum remains intact
```

These are expected sell zones, not guaranteed sell prices.

## Exit Reviews

Exit-like signals are framed as paper reviews:

- stop-loss paper exit review
- take-profit paper exit review
- momentum failure paper exit review
- risk-based exposure review

These are not automatic real-money orders. They are prompts to review the paper plan, risk exposure, and journal context.
