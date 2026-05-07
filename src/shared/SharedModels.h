#pragma once

#include "../StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct SharedModelField
    {
        std::string name;
        std::string type;
        bool required = true;
        std::string detail;
    };

    struct SharedModelContract
    {
        std::string name;
        std::string version;
        std::string owner;
        std::string detail;
        std::vector<SharedModelField> fields;
    };

    std::string SharedModelContractVersion();
    std::vector<SharedModelContract> BuildSharedModelContracts();
    std::vector<InfoItem> BuildSharedModelContractRows();
}
