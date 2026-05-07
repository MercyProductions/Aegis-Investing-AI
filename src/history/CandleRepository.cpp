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
            item.tag = "candle-repository";
            return item;
        }
    }

    std::vector<InfoItem> BuildCandleRepositoryRows(const HistoricalCandlesResult& result)
    {
        return {
            Row("Loaded candles", result.ok ? "Available" : "Missing", std::to_string(static_cast<int>(result.candles.size())), result.status.empty() ? "No historical load has completed." : result.status, result.ok ? "Ready" : "Review"),
            Row("Repository source", result.source.empty() ? "Unloaded" : result.source, result.live ? "Provider" : result.cached ? "Cache" : "Fallback", (result.fetched_at.empty() ? "--" : result.fetched_at) + ". " + (result.cache_age_label.empty() ? "Source metadata will be required for every candle bundle." : result.cache_age_label), result.live || result.cached ? "Ready" : "Review"),
            Row("Fallback reason", result.fallback_reason.empty() ? "None" : "Visible", result.fallback_reason.empty() ? "--" : result.fallback_reason, "Fallback reasons stay attached to candle data so analytics does not appear anonymous.", result.fallback_reason.empty() ? "Ready" : "Review")
        };
    }
}
