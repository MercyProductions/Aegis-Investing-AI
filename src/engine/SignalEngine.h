#pragma once

#include "../StockData.h"

#include <vector>

namespace aegis
{
    struct SignalEngineSnapshot
    {
        int signal_count = 0;
        int high_conviction = 0;
        int caution_count = 0;
        double average_score = 0.0;
        std::vector<InfoItem> rows;
    };

    SignalEngineSnapshot BuildSignalEngineSnapshot(const StockState& state);
    std::vector<InfoItem> BuildSignalEngineRows(const StockState& state);
}
