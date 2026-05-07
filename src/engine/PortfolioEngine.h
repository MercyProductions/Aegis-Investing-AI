#pragma once

#include "../StockData.h"

#include <vector>

namespace aegis
{
    struct PortfolioEngineSnapshot
    {
        double market_value = 0.0;
        double cost_basis = 0.0;
        double unrealized_pnl = 0.0;
        double cash = 0.0;
        std::vector<InfoItem> rows;
    };

    PortfolioEngineSnapshot BuildPortfolioEngineSnapshot(const StockState& state, const std::vector<PortfolioHolding>& holdings, double cash);
    std::vector<InfoItem> BuildPortfolioEngineRows(const StockState& state, const std::vector<PortfolioHolding>& holdings, double cash);
}
