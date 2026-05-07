#pragma once

#include "../providers/IMarketProvider.h"

#include <string>
#include <vector>

namespace aegis
{
    struct CoreProviderSnapshot
    {
        std::string owner = "AuralithCore";
        std::string source;
        std::string freshness;
        std::string fetched_at;
        int quote_count = 0;
        int confidence = 0;
        bool live = false;
        bool fallback = false;
        bool delayed = false;
        std::vector<InfoItem> rows;
        MarketProviderResponse response;
    };

    CoreProviderSnapshot BuildCoreProviderSnapshot(const Config& config, const StockState& state);
    std::vector<InfoItem> BuildCoreExtractionRoadmapRows();
    std::vector<InfoItem> BuildCoreSharedIntelligenceRows(const Config& config, const StockState& state);
}
