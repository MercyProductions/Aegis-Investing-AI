#include "HistoricalStore.h"

namespace aegis
{
    namespace
    {
        InfoItem Row(const std::string& name, const std::string& label, const std::string& value, const std::string& detail)
        {
            InfoItem item;
            item.name = name;
            item.label = label;
            item.value = value;
            item.detail = detail;
            item.state = "Ready";
            item.tag = "retention";
            return item;
        }
    }

    std::vector<InfoItem> BuildDataRetentionRows(const Config& config)
    {
        return {
            Row("History TTL", "Configured", std::to_string(config.history_cache_hours) + " hours", "Cached history refreshes after the configured TTL unless force refresh is enabled."),
            Row("Research TTL", "Configured", std::to_string(config.research_cache_hours) + " hours", "Research bundles should use separate retention because news/earnings decay differently from candles."),
            Row("Cache budget", "Configured", std::to_string(config.max_cache_mb) + " MB", "DataRetention owns pruning policy and cache pressure reporting."),
            Row("Compression", "Planned", "SQLite/blob", "Future historical storage can compress older bars while keeping recent windows fast.")
        };
    }
}
