# Auralith Cross-Platform Web Platform

This phase makes the web cockpit feel like a serious Auralith client on desktop browsers, laptops, tablets, phones, and installable PWA shells while the Windows desktop terminal remains the advanced reference workstation.

## Product Role

- Desktop terminal: dense research workstation, experimentation surface, and advanced workflow reference.
- Web platform: polished universal-access client for dashboards, watchlists, alerts, reports, symbol research, portfolio review, journal context, and settings.
- Mobile shell: focused monitoring surface for risk state, alerts, reports, provider freshness, and selected-symbol research.

The web app should keep consuming AuralithCore contracts. Do not fork scanner logic, risk logic, provider normalization, report generation, or paper execution rules into React-specific business logic.

## Implemented In This Pass

- Installable PWA metadata with Auralith icons, shortcuts, standalone display, scope, theme color, and maskable icon support.
- Service worker offline shell for the app frame and brand assets.
- Service worker push notification handler for future server push delivery.
- Last-good `AppState` persistence in browser storage so the web cockpit can restore a saved session when AuralithCore or the network is unavailable.
- Selected-symbol persistence across launches.
- Online/offline and saved-session badges in the top bar and dashboard.
- Mobile bottom navigation for high-frequency routes: dashboard, watchlists, portfolio, research, reports, and alerts.
- Mobile-first dashboard summary cards for portfolio risk, top signal, alert state, and report readiness.
- Alert page notification readiness panel with browser permission flow.
- Settings panels for account sync coverage, security hardening boundaries, and release polish state.
- Responsive CSS improvements for dense desktop, tablet split research, and focused phone card layouts.
- Table/chart overflow handling for small screens.

## Platform UX Targets

Desktop browser:
- Use the dense terminal layout.
- Keep sidebar, top bar, context rail, scanner tables, and portfolio tables visible.
- Prioritize keyboard navigation and command palette access.

Tablet:
- Use split-panel symbol research where space allows.
- Keep charts and research context side-by-side when practical.
- Collapse secondary dashboard grids into readable single or two-column panels.

Phone:
- Use focused card-based monitoring.
- Prefer the mobile bottom nav over the desktop sidebar.
- Keep the context rail hidden and surface priority status in the dashboard cards.
- Preserve horizontal scrolling for dense scanner and holdings tables rather than crushing labels.

## Sync Boundary

Current sync status is local-first and Core-contract based:
- Watchlists, reports, alerts, portfolio, journal, provider health, scanner results, symbol research, paper orders, and AI summaries arrive through the AuralithCore web contract.
- Browser session cache stores the last good web state for offline resilience.
- Future account sync should move through AuralithCore or a shared sync service, not direct page-local business logic.

## Notification Boundary

Current notification readiness:
- Browser notification permission is available from the Alerts page.
- Service worker can display push payloads when a future server push channel exists.
- Current in-app notification center remains the source of truth for alert, stale-provider, risk, report, paper-order, and earnings-style events.

Future push delivery needs:
- Server-side subscription storage.
- VAPID or equivalent push credentials.
- User/device preference routing.
- Quiet hours and notification category controls.
- Audit events for delivered, acknowledged, snoozed, and failed notifications.

## Security Boundary

This pass exposes the security surfaces without claiming complete production security:
- Branded auth, 2FA setup, and device history screens exist.
- Settings now lists security hardening boundaries.
- Future work must implement secure cookies, CSRF policy, rate limits, provider-key encryption, server-side session/device revocation, and audit-backed security events.

## Performance Boundary

Current performance improvements:
- Responsive layout avoids forcing the full desktop terminal shape onto phones.
- Panels use `content-visibility` to reduce offscreen rendering work in long dashboard pages.
- Dense tables preserve horizontal scroll on small screens.
- PWA shell caches static assets.

Future work:
- Virtualize large scanner/watchlist/portfolio tables.
- Split routes with lazy loading.
- Add websocket/SSE updates for provider and alert events.
- Add API response caching policy and stale-while-revalidate behavior.
- Track bundle size budgets.

## Validation Checklist

- `cd website`
- `npm run typecheck`
- `npm run build`
- `powershell -ExecutionPolicy Bypass -File ..\scripts\launch-website.ps1`
- Verify `/api/health`
- Verify `/api/core/dashboard`
- Verify dashboard at desktop width.
- Verify symbol research at tablet width.
- Verify dashboard, watchlists, alerts, portfolio, reports, and symbol research at phone width.
- Verify install prompt metadata in a Chromium browser.
- Verify the app reloads with the last saved session if the API is unavailable.
