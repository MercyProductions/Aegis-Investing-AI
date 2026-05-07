# Auralith Installer And Deployment Plan

Auralith should install like a professional local-first research platform.

## Release Targets

- Stable
- Beta
- Dev
- Portable

## Installer Responsibilities

- Install `AegisStockBettingAI.exe` as the Auralith desktop terminal until the executable is renamed.
- Install `AuralithCore.exe` as the optional local core service/daemon.
- Register app icon, product metadata, and release notes.
- Preserve `%LOCALAPPDATA%\Aegis\Stock Betting AI` data during upgrades.
- Run database backup checks before schema migrations.
- Support clean uninstall without silently deleting user research data.

## Production Requirements

- Signed binaries
- Signed installer
- Rollback artifacts
- Release notes
- Safe-mode launch shortcut
- Portable mode package
- Offline repair instructions

The updater should not be built until service identity, backup semantics, and release channels are stable.
