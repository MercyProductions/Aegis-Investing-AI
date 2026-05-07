#pragma once

#include "../StockData.h"

#include <vector>

namespace aegis
{
    struct RiskEngineSnapshot
    {
        double gross_exposure = 0.0;
        double largest_position_percent = 0.0;
        int concentration_warnings = 0;
        int liquidity_warnings = 0;
        std::vector<InfoItem> rows;
    };

    RiskEngineSnapshot BuildRiskEngineSnapshot(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings);
    std::vector<InfoItem> BuildRiskEngineRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings);
}
