# Auralith SDK

The SDK is an internal boundary for future integrations, scripts, plugins, provider adapters, scanners, indicators, and report generators.

The first SDK surfaces are:

- MarketDataAPI
- PortfolioAPI
- AlertAPI
- ReportAPI
- ScannerAPI
- HistoricalAPI
- PluginHost

The SDK is local-first and permission-gated. It intentionally does not expose live trading permissions.

## Rules

- Plugins and scripts consume shared model contracts from `Shared/Models/auralith-contracts.schema.json`.
- Every mutation should produce an audit event.
- Provider secrets should remain isolated behind the secret provider.
- Paper execution remains gated by paper-only mode and manual confirmation.
- Data outputs must carry source, timestamp, freshness, fallback, and confidence metadata.
