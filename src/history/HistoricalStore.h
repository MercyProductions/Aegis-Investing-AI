#pragma once

#include "../AdvancedAnalytics.h"

#include <filesystem>
#include <vector>

namespace aegis
{
    struct HistoricalStoreSnapshot
    {
        std::filesystem::path root;
        int retention_days = 0;
        int cache_budget_mb = 0;
        std::vector<InfoItem> rows;
    };

    HistoricalStoreSnapshot BuildHistoricalStoreSnapshot(const Config& config);
    std::vector<InfoItem> BuildHistoricalStoreRows(const Config& config);
    std::vector<InfoItem> BuildCandleRepositoryRows(const HistoricalCandlesResult& result);
    std::vector<InfoItem> BuildDataRetentionRows(const Config& config);
    std::vector<InfoItem> BuildHistoricalSyncRows(const Config& config, const StockState& state);
}
