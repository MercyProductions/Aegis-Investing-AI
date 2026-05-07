# AuralithCore Extraction Plan

AuralithCore is the shared intelligence layer that should eventually power the desktop terminal, web cockpit, and future mobile companion.

The desktop app remains the primary advanced implementation. It should continue to validate dense research workflows before those workflows are promoted into shared Core services and exposed to the website.

## Target Shape

```text
Shared AuralithCore
    Provider normalization
    Scanner and momentum logic
    Alerts
    Portfolio and risk
    Reports
    AI summaries and memory
    Historical storage
        |
        +-- Desktop terminal
        +-- Web cockpit
        +-- Future mobile companion
```

Different clients should render and command the same intelligence layer. They should not reimplement signal generation, momentum setup logic, risk analysis, provider normalization, AI summaries, or report generation independently.

## Extraction Order

### 1. Provider Layer

Status: In progress.

Core responsibilities:

- Provider routing
- Provider health
- Provider normalization
- Cache/fallback labeling
- Source and fetched-at metadata
- Freshness scoring
- Confidence scoring
- Rate-limit and retry policy

Current bridge:

- `src/core/CoreIntelligence.*` publishes a Core provider snapshot.
- `src/providers/IMarketProvider.h` defines provider response structures.
- `src/providers/*Provider.cpp` supplies provider health and adapter scaffolds.
- `src/ProviderLayer.*` still owns policy helpers and diagnostics used by both apps.

Next step:

- Move live provider orchestration out of UI paths and into Core-owned provider request execution.

### 2. Scanner Engine

Status: Next.

Core should own:

- Signal generation
- Momentum score
- Paper setup plan
- Entry, invalidation, stop, expected sell zones, and trailing stop
- Conviction and risk labels
- Data-quality confidence attached to scanner output

The desktop can keep advanced scanner UI and experimentation, but the scoring output should come from Core.

### 3. Alert Engine

Status: Queued.

Core should own:

- Alert evaluation
- Trigger history
- Snooze/acknowledge state
- Provider freshness used for alert checks
- Notification dispatch
- Audit records

### 4. Portfolio Engine

Status: Planned.

Core should own:

- Paper portfolio state
- Cost basis
- Realized and unrealized P/L
- Exposure
- Concentration
- Portfolio risk summaries
- Portfolio-linked journal sync

### 5. Report Engine

Status: Planned.

Core should own:

- Morning briefing
- Risk report
- Portfolio review
- Strategy report
- HTML/PDF report generation
- Report audit records

### 6. AI Systems

Status: Planned.

Core should own:

- Research summaries
- Risk narratives
- Portfolio reviews
- Watchlist analyst notes
- AI memory and research continuity
- Cautious language constraints

### 7. Historical Storage

Status: Planned.

Core should own:

- Candle store
- Indicator snapshots
- Retention policy
- Historical validation
- Missing-data recovery
- Backtest data access

## Rules

- Do not abandon the desktop app.
- Do not duplicate business logic across desktop and web.
- Do not force web parity before a workflow is stable in desktop and extracted into Core.
- Do not let clients bypass paper-only or manual-confirmation safety gates.
- Do not let clients render decision-impacting data without source, freshness, fallback, and confidence metadata.
- Do not imply financial advice, guaranteed outcomes, or live autonomous trading.

## Promotion Standard

A system is ready to move from desktop-only to shared Core when:

- The behavior is proven in the desktop terminal.
- It has clear inputs and outputs.
- It has tests.
- It exposes source/freshness/confidence where market data matters.
- It writes audit events for important actions.
- It can serve desktop and web without UI-specific assumptions.

