# Auralith Professional Platform

This phase turns Auralith from a strong local research terminal into a professional investment intelligence ecosystem.

The next mature step is selective refinement, not unchecked expansion. The product doctrine lives in `docs/PRODUCT_DOCTRINE.md`, the platform split lives in `docs/PLATFORM_STRATEGY.md`, the Core extraction plan lives in `docs/CORE_EXTRACTION_PLAN.md`, the long-term quality model lives in `docs/LONGEVITY_CHARTER.md`, and mature stewardship guidance lives in `docs/STEWARDSHIP_MODE.md`. Use them as filters before adding more panels, workflows, SDK surfaces, or AI behavior.

## Target Architecture

The long-term layout is:

- `Core/`
- `Desktop/`
- `Web/`
- `Shared/`
- `Providers/`
- `Plugins/`
- `Reports/`
- `AI/`
- `Data/`
- `Docs/`
- `SDK/`
- `Installer/`

The current source tree keeps the working Visual Studio project stable while new boundaries are introduced under `src/shared`, `src/sdk`, and `src/professional`.

Keep the desktop terminal as the advanced reference implementation while the web dashboard evolves as a cleaner companion cockpit. Shared intelligence should move into engines, services, contracts, and AuralithCore/local API boundaries before workflows are promoted broadly across UI layers.

## Current Foundation

- Shared C++ model contracts: `src/shared/SharedModels.*`
- Shared JSON contracts: `Shared/Models/auralith-contracts.schema.json`
- Internal SDK surface: `src/sdk/AuralithSDK.*`
- Professional maturity diagnostics: `src/professional/ProfessionalPlatform.*`
- Plugin manifest schema: `plugins/plugin.schema.json`
- Installer planning: `Installer/README.md`

## Maturity Priorities

- Keep Auralith local-first.
- Keep paper execution and manual confirmation mandatory.
- Require source, freshness, fallback, and confidence metadata.
- Build extension points around explicit permissions.
- Add installer/update infrastructure only after backup and rollback behavior is boring and reliable.

## Refinement Priorities

- Decide whether each surface strengthens the primary identity: an AI-assisted portfolio research terminal for long-term daily use.
- Prefer merging, simplifying, or removing overlapping panels before adding new ones.
- Improve data correctness, confidence scoring, and stale-data behavior before adding more analytics.
- Make AI explanations more useful by connecting memory, history, portfolio context, and risk changes.
- Treat local-first ownership, paper-first safety, and research discipline as product constraints, not optional settings.

## Expansion Gate

New features should clear this gate:

- They reduce cognitive load or materially improve a core workflow.
- They strengthen AI memory, research continuity, risk intelligence, historical context, reporting, or reliability.
- They expose source/freshness/confidence clearly.
- They do not create live-trading ambiguity.
- They do not make Auralith feel like a hype dashboard or gambling app.

## Longevity Gate

Once Auralith is feature-complete enough, new work should usually be rejected unless it improves:

- Signal quality
- AI reasoning quality
- Portfolio insight quality
- Historical continuity
- Workflow smoothness
- Data trustworthiness
- System resilience
- User confidence

## Stewardship Gate

At functional maturity, prefer maintenance and refinement unless the work clearly improves:

- Research quality
- Intelligence continuity
- Portfolio understanding
- Risk visibility
- Workflow efficiency
- Trustworthiness
- Maintainability

Novelty alone is not enough.
