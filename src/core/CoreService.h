#pragma once

#include "../StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct CoreServiceStatus
    {
        bool ok = false;
        std::string mode;
        std::string database;
        std::string status;
        std::string provider_source;
        std::string provider_freshness;
        int quote_count = 0;
        int provider_confidence = 0;
        std::vector<InfoItem> rows;
    };

    CoreServiceStatus RunCoreServiceCycle(bool refresh_providers);
    CoreServiceStatus BuildCoreServiceHealth();
    std::string CoreServiceStatusJson(const CoreServiceStatus& status);
}
