#pragma once

#include "../StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct WorkspaceProfile
    {
        std::string name;
        std::string focus;
        std::string layout;
        std::string scanner_preset;
        std::string risk_profile;
    };

    std::vector<WorkspaceProfile> BuildDefaultWorkspaceProfiles();
    std::vector<InfoItem> BuildWorkspaceProfileRows();
}
