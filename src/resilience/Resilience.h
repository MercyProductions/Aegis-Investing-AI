#pragma once

#include "../StockData.h"

#include <vector>

namespace aegis
{
    std::vector<InfoItem> BuildResilienceRows(const Config& config, const StockState& state);
    std::vector<InfoItem> BuildRecoveryPlanRows();
}
