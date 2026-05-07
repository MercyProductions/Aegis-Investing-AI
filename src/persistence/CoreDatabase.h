#pragma once

#include "../StockData.h"

#include <filesystem>
#include <string>
#include <vector>

namespace aegis
{
    struct CoreDatabaseResult
    {
        bool ok = false;
        std::filesystem::path path;
        int schema_version = 0;
        std::string status;
        std::string error;
        std::vector<InfoItem> rows;
    };

    std::filesystem::path AuralithCoreDatabasePath();
    std::string AuralithCoreSchemaSql();
    CoreDatabaseResult InitializeAuralithCoreDatabase(const std::filesystem::path& path = {});
    bool PersistCoreServiceSnapshot(const StockState& state, const std::vector<InfoItem>& rows, const std::filesystem::path& path = {}, std::string* status = nullptr);
    std::vector<InfoItem> BuildCoreDatabaseRows(const std::filesystem::path& path = {});
    bool RunCoreDatabaseSelfTest(std::string& status);
}
