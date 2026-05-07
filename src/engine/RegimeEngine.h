#pragma once

#include "../StockData.h"

#include <vector>

namespace aegis
{
    struct RegimeEngineSnapshot
    {
        std::string momentum_regime = "Neutral";
        std::string volatility_regime = "Normal";
        std::string risk_regime = "Balanced";
        std::vector<InfoItem> rows;
    };

    RegimeEngineSnapshot BuildRegimeEngineSnapshot(const StockState& state);
    std::vector<InfoItem> BuildRegimeEngineRows(const StockState& state);
}
