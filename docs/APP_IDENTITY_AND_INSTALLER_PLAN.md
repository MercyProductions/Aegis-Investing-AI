# Auralith App Identity And Installer Plan

## Identity Added In This Pass

- Product name: Auralith
- Desktop title: Auralith Research Cockpit
- Web title: Auralith Markets
- Version resource: `src/Auralith.rc`
- Product version placeholder: `0.2.0`
- App icon source: `assets/logo.png`
- App icon resource: `assets/auralith.ico`
- Release notes: `RELEASE_NOTES.md`

The executable and Visual Studio project names remain `AegisStockBettingAI` for this pass so the current build path and release scripts keep working. A later rename can move project files after the storage migration and installer have compatibility checks.

## Installer Plan

Recommended packaging order:

1. Add a clean release layout under `dist/Auralith/<version>/`.
2. Copy the native executable, config template, docs, release notes, schema, website launch script, and bundled website production output.
3. Add an installer script using Inno Setup or WiX.
4. Install into `%LOCALAPPDATA%\Programs\Auralith` by default for user-level installs.
5. Preserve the legacy data folder until the SQLite migration has completed and been audited.
6. Add Start Menu shortcuts for Auralith Research Cockpit and Auralith Markets.
7. Add uninstall behavior that never deletes user data without explicit confirmation.

## Update Channel Placeholder

Future updater metadata should use:

```text
channel=dev
product=Auralith
version=0.2.0
feed_url=
```

No automatic updater is implemented in this pass. The placeholder exists so future release tooling has a stable field model.
