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
            item.tag = "history";
            return item;
        }
    }

    HistoricalStoreSnapshot BuildHistoricalStoreSnapshot(const Config& config)
    {
        HistoricalStoreSnapshot snapshot;
        snapshot.root = AppDataDirectory() / "cache" / "history";
        snapshot.retention_days = config.history_cache_hours / 24;
        snapshot.cache_budget_mb = config.max_cache_mb;
        snapshot.rows = {
            Row("Historical store", "Centralized", snapshot.root.string(), "Charts, analytics, reports, and backtests should read through this store boundary."),
            Row("Incremental sync", "Planned", std::to_string(config.history_cache_hours) + "h TTL", "HistoricalSync will request only missing windows and validate returned candles."),
            Row("Offline resilience", "Cache-first", std::to_string(snapshot.cache_budget_mb) + " MB", "Cached candles allow chart and report workflows to degrade gracefully when providers fail."),
            Row("Validation", "Required", "OHLCV checks", "CandleRepository should reject malformed, inverted, or duplicate bars before they reach analytics.")
        };
        return snapshot;
    }

    std::vector<InfoItem> BuildHistoricalStoreRows(const Config& config)
    {
        return BuildHistoricalStoreSnapshot(config).rows;
    }
}
