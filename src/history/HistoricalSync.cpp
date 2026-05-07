#include "HistoricalStore.h"

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
            item.tag = "historical-sync";
            return item;
        }
    }

    std::vector<InfoItem> BuildHistoricalSyncRows(const Config& config, const StockState& state)
    {
        return {
            Row("Sync universe", state.quotes.empty() ? "Empty" : "Ready", std::to_string(static_cast<int>(state.quotes.size())) + " symbols", "HistoricalSync should share the same symbol universe as MarketEngine."),
            Row("Provider route", config.alpha_vantage_api_key.empty() ? "Fallback" : "Alpha Vantage", config.force_live_refresh ? "Force live" : "Cache-aware", "Incremental sync will prefer fresh cache, then configured provider, then labeled fallback.", config.alpha_vantage_api_key.empty() ? "Review" : "Ready"),
            Row("Missing data recovery", "Planned", "Gap fill", "Repository validation should mark missing sessions and request only the gaps."),
            Row("Long-running behavior", "Required", "Non-blocking", "Historical loads should run through BackgroundTaskManager so the terminal stays responsive.")
        };
    }
}
