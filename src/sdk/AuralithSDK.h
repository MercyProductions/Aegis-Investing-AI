#pragma once

#include "../StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct SdkModule
    {
        std::string name;
        std::string surface;
        std::string permission;
        std::string stability;
        std::string detail;
    };

    std::string AuralithSdkVersion();
    std::vector<SdkModule> BuildSdkModules();
    std::vector<InfoItem> BuildSdkModuleRows();
    std::vector<InfoItem> BuildSdkPermissionRows();
}
