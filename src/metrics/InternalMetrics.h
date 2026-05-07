#pragma once

#include "../StockData.h"

#include <vector>

namespace aegis
{
    std::vector<InfoItem> BuildInternalMetricsRows(const Config& config, const StockState& state);
}
