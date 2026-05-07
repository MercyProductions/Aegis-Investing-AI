#pragma once

#include "../StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct MarketEngineSnapshot
    {
        std::string session_state;
        std::string source_label;
        std::string freshness_label;
        int symbols = 0;
        int live_symbols = 0;
        int fallback_symbols = 0;
        int stale_symbols = 0;
        int average_confidence = 0;
        std::vector<InfoItem> rows;
    };

    MarketEngineSnapshot BuildMarketEngineSnapshot(const Config& config, const StockState& state);
    std::vector<InfoItem> BuildMarketEngineRows(const Config& config, const StockState& state);
}
