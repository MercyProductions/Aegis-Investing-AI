# Auralith Release Notes

## 0.2.0 - Auralith professionalization pass

- Rebranded user-facing native and web cockpit surfaces to Auralith.
- Repositioned the product as a market intelligence, research cockpit, and paper execution platform.
- Kept paper-only and manual-confirmation guardrails visible in desktop and web UI.
- Extracted the Settings tab into `src/ui/SettingsTab.*` as the first tab-module step out of `src/Main.cpp`.
- Added `src/services/SafetyGate.*` for central native guardrail status.
- Added `src/services/BackgroundTaskManager.*` for centralized task status rows.
- Added web server safety gate enforcement for paper orders, arbitrage simulations, and live execution blocking.
- Added provider freshness/source labels to the selected-symbol panel, header, web top bar, and web scanner rows.
- Added SQLite migration map and app identity/installer planning docs.
- Added Windows version metadata and an Auralith icon resource while keeping the legacy executable filename for compatibility.

Known follow-ups:

- Continue extracting `Main.cpp` tab modules one at a time.
- Implement the actual native SQLite runtime and backup/import executor.
- Add Windows toast notifications behind the existing Settings toggle.
- Build the installer after release layout and storage compatibility are stable.
