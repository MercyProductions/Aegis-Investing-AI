# Auralith Platform Strategy

Auralith should keep the desktop terminal as the primary advanced implementation while the website matures gradually into a polished companion cockpit.

The goal is not to abandon the native app, rewrite stable systems for the browser, or force premature feature parity. The goal is different UI layers over a shared intelligence core.

## Platform Roles

### Desktop Terminal

The desktop app is the reference implementation for advanced workflows.

Use it as:

- The experimentation platform
- The workflow validation platform
- The advanced feature platform
- The dense research workstation
- The first home for complex terminal UX

The desktop app should continue to lead on:

- Deep symbol research
- Chart lab workflows
- Advanced scanner behavior
- Portfolio and risk workflow validation
- Paper execution modeling
- Report generation
- Journal and research continuity
- Diagnostics and provider operations
- Long-session reliability testing

### Web Cockpit

The website should become more capable over time, but it should remain cleaner and more focused than the desktop terminal.

Use it as:

- A fast market intelligence cockpit
- A remote monitoring surface
- A polished reports and alerts hub
- A portfolio review surface
- A lightweight symbol research companion
- A usability-focused expression of mature desktop workflows

The website should prioritize:

- Polish
- Readability
- Responsive layout
- Command-center workflows
- Monitoring and review
- Carefully promoted desktop workflows
- Clear source, freshness, fallback, and confidence labels

It should not try to become a full terminal in the browser before the shared engine is ready.

## Shared Intelligence Core

Desktop and web should increasingly share:

- Backend services
- Market engines
- Provider systems
- Scanner logic
- AI systems
- Portfolio systems
- Alert systems
- Reporting systems
- Shared model contracts
- Safety and audit rules

Business logic belongs in shared services, engines, contracts, or the local core service. UI layers should render engine outputs and submit explicit commands.

## Promotion Path

When a desktop workflow is mature enough for the web, promote it through this path:

1. Validate the workflow in the desktop terminal.
2. Extract reusable business logic into an engine, service, or shared model.
3. Add tests for the behavior before exposing it in another UI.
4. Expose the output through the local API, shared contract, or core service boundary.
5. Build a focused web presentation that fits the web cockpit role.
6. Verify source, freshness, safety, and audit behavior are identical across platforms.

Feature parity should mean behavior and trust parity first, not identical screens.

## Parity Levels

Use these levels when deciding what belongs on web:

- Level 0: Desktop-only advanced workflow.
- Level 1: Web can monitor the workflow output.
- Level 2: Web can review, filter, acknowledge, export, or open reports.
- Level 3: Web can safely perform the workflow through shared services.
- Level 4: Keep desktop-only because the workflow needs dense terminal UX, long-running analysis, or complex interaction.

Most new web work should start at Level 1 or Level 2. Move to Level 3 only after the shared core is stable.

## Architecture Rules

- Do not duplicate business logic between desktop and web.
- Do not rewrite stable native systems just because a web surface exists.
- Do not force browser parity before workflows are proven.
- Do not bloat the web dashboard with advanced controls that belong in desktop.
- Do not let web-only shortcuts bypass paper-only or manual-confirmation guardrails.
- Do not allow anonymous market data to enter either UI.
- Do not let UI copy imply financial advice, guaranteed outcomes, or live autonomous trading.

## Practical Next Steps

- Treat `src/engine`, `src/services`, `src/providers`, `src/reports`, `src/ai`, `src/paper`, `src/audit`, and `src/workflows` as the source of truth for shared behavior.
- Treat `Shared/Models/auralith-contracts.schema.json` as the contract target for cross-platform model alignment.
- Continue extracting desktop logic out of `src/Main.cpp` before attempting broad web parity.
- Keep `website/src/server` as a bridge layer until AuralithCore/local API becomes the durable shared backend.
- Keep `website/src/client` focused on dashboard, reports, alerts, watchlists, portfolio review, and lightweight research.
- Prefer adding shared endpoints and contracts before adding duplicate TypeScript calculations.

## Acceptance Standard

A web feature is ready when:

- The business logic lives in a shared engine/service or stable API boundary.
- The web UI is simpler than the desktop version unless the workflow genuinely needs depth.
- Paper-only mode and manual confirmation remain visible.
- Data freshness, provider, fallback, and confidence metadata are shown where decisions are affected.
- Audit behavior matches the desktop/core path.
- Tests cover the shared behavior, not just the browser rendering.

## Product Principle

Desktop momentum matters.

The website should mature by adopting proven workflows from the desktop terminal, not by competing with it or rebuilding it from scratch.

Auralith should become:

> Different UI layers over one disciplined market intelligence core.

