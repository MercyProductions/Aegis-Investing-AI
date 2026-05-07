#pragma once

#include "../AdvancedAnalytics.h"

#include <string>
#include <vector>

namespace aegis
{
    struct AnalyticsEngineSnapshot
    {
        double sharpe = 0.0;
        double sortino = 0.0;
        double volatility = 0.0;
        double max_drawdown = 0.0;
        double rolling_return = 0.0;
        std::vector<InfoItem> rows;
    };

    AnalyticsEngineSnapshot BuildAnalyticsEngineSnapshot(const std::vector<Candle>& candles, const std::string& label);
    std::vector<InfoItem> BuildAnalyticsEngineRows(const std::vector<Candle>& candles, const std::string& label);
}
