# Auralith Plugin Architecture

This directory reserves the local-first extension surface for future Auralith modules:

- `indicators/`: custom indicators and factor calculations
- `scanners/`: custom scanner rules and ranking models
- `reports/`: custom report templates and exporters
- `providers/`: market/news/fundamental provider adapters
- `layouts/`: workspace layout presets

Plugins must remain sandboxed where practical. They should not receive plaintext provider secrets by default, should write through the audit layer, and must preserve Auralith's research-first and paper-first safety boundaries.
