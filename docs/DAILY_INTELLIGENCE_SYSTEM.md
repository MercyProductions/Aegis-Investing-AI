# Auralith Daily Intelligence System

Auralith is being shaped as a daily-use, research-first investment operating system. The core daily loop is:

1. Morning Briefing: prepare before market open.
2. Live Market Command Center: monitor during market hours.
3. End-of-Day Review: summarize what happened and update journal context.
4. Weekly Portfolio Review: evaluate portfolio health, risk, strategy quality, and next-week watchlist.

The system remains paper-first and safety-forward. It must not imply financial advice, guaranteed returns, autonomous live trading, or certainty.

## Morning Briefing

The Morning Briefing answers:

- What changed overnight?
- What should I watch today?
- Which holdings need attention?
- Which watchlist symbols moved?
- Which earnings/events matter today?
- Which alerts are close to triggering?
- What is the market regime?
- What are the biggest risks?

Each row should explain why it matters and show source/freshness context.

## Live Market Command Center

The command center is the market-hours control room. It combines:

- Global market ribbon
- Active watchlist
- Scanner stream
- Triggered alerts
- Selected-symbol chart
- Position exposure
- Paper order queue
- Risk guardrails
- Provider health
- Market session state

Focus modes:

- Portfolio Monitor
- Watchlist Scanner
- Earnings Mode
- Volatility Mode
- Paper Execution Mode
- Research Mode

## Reviews

End-of-Day Review summarizes P/L, paper order outcomes, alerts, signal accuracy, missed opportunities, risk violations, and journal mistakes.

Weekly Review grades portfolio health, concentration, cash allocation, drawdown, benchmark readiness, paper strategy performance, repeated journal mistakes, and next-week watchlist candidates.

## Strategy Lab

Strategy Lab supports the disciplined paper-strategy workflow:

- Rule creation/editing
- Closed-candle backtests
- Paper execution simulation
- Strategy comparison
- Saved presets
- Live paper performance tracking

Rules should support moving averages, RSI, MACD, volume spikes, gaps, earnings windows, volatility filters, benchmark filters, market regime filters, risk limits, and stop/target logic.

## Risk And Data Quality

Risk states are:

- Healthy
- Caution
- Elevated
- Defensive
- Critical

Each state must explain its drivers.

Data quality states are:

- Fresh
- Delayed
- Stale
- Partial
- Fallback
- Failed

Any scanner score, risk warning, portfolio grade, report, or paper setup should include confidence language based on data quality.

## Command Palette

Ctrl+K focuses the command field. Supported workflow commands include:

- morning
- command center
- eod
- weekly
- strategy lab
- risk
- symbol
- refresh
- report
- add TICKER
- search TICKER
- alert TICKER above PRICE

## Release Quality

The Daily Intelligence phase is complete only when native build/tests, web typecheck/build, provider failure simulation, stale data simulation, migration test, paper order lifecycle test, report export test, and long-running session tests pass.
