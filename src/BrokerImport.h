#pragma once

#include "StockData.h"

#include <filesystem>
#include <string>
#include <vector>

namespace aegis
{
    struct BrokerImportProfile
    {
        std::string name;
        std::string file_name;
        std::vector<std::string> symbol_headers;
        std::vector<std::string> shares_headers;
        std::vector<std::string> average_cost_headers;
        std::vector<std::string> total_cost_headers;
        std::vector<std::string> account_headers;
        std::vector<std::string> type_headers;
    };

    struct BrokerImportRow
    {
        std::string profile;
        std::string source_file;
        int row_number = 0;
        std::string symbol;
        double shares = 0.0;
        double average_cost = 0.0;
        std::string account;
        std::string note;
        bool valid = false;
        std::string status;
        std::string error;
    };

    std::vector<BrokerImportProfile> BrokerImportProfiles();
    std::vector<BrokerImportRow> PreviewBrokerImport(const std::filesystem::path& directory);
    int CountValidBrokerRows(const std::vector<BrokerImportRow>& rows);
    std::string BrokerImportInstructions();
}
