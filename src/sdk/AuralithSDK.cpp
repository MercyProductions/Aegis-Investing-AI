#include "AuralithSDK.h"

namespace aegis
{
    namespace
    {
        InfoItem Row(const std::string& name, const std::string& label, const std::string& value, const std::string& detail, const std::string& state = "Ready")
        {
            InfoItem item;
            item.name = name;
            item.label = label;
            item.value = value;
            item.detail = detail;
            item.state = state;
            item.tag = "sdk";
            item.source = "Auralith SDK";
            return item;
        }
    }

    std::string AuralithSdkVersion()
    {
        return "sdk-draft-v1";
    }

    std::vector<SdkModule> BuildSdkModules()
    {
        return {
            { "MarketDataAPI", "quotes, candles, provider health, freshness", "market-data:read", "Draft", "Read-only provider-normalized data access with source/freshness metadata." },
            { "PortfolioAPI", "paper portfolios, positions, exposure", "portfolio:read", "Draft", "Paper portfolio access for reports, dashboards, and review workflows." },
            { "AlertAPI", "alerts, notifications, snooze/ack", "alerts:read/write", "Draft", "Research alert workflow access with audit events on mutation." },
            { "ReportAPI", "HTML/PDF report composition", "reports:read/write", "Draft", "Report templates and exports without direct provider secret access." },
            { "ScannerAPI", "scanner outputs, custom factors", "scanner:read/write", "Draft", "Custom scanners must declare data dependencies and output confidence." },
            { "HistoricalAPI", "candles, indicators, replay windows", "history:read", "Draft", "Historical data access through retention and quality checks." },
            { "PluginHost", "manifest, permissions, sandbox boundary", "plugins:install", "Planning", "Plugins declare permissions and avoid unrestricted execution." }
        };
    }

    std::vector<InfoItem> BuildSdkModuleRows()
    {
        std::vector<InfoItem> rows;
        for (const SdkModule& module : BuildSdkModules())
            rows.push_back(Row(module.name, module.stability, module.surface, module.detail));
        rows.push_back(Row("SDK version", "Internal", AuralithSdkVersion(), "The SDK remains local-first and research-first; no live execution capability is exposed."));
        return rows;
    }

    std::vector<InfoItem> BuildSdkPermissionRows()
    {
        std::vector<InfoItem> rows;
        for (const SdkModule& module : BuildSdkModules())
            rows.push_back(Row(module.permission, module.name, module.stability, "Permission required before this SDK surface can be used by scripts, plugins, or external integrations."));
        rows.push_back(Row("live-trading:write", "Unavailable", "Blocked", "The professional SDK intentionally exposes no live execution permission.", "Locked"));
        return rows;
    }
}
