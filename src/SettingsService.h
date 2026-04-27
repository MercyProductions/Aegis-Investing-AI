#pragma once

#include "StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct SettingsInput
    {
        std::string watchlist;
        std::string alpha_vantage_api_key;
        std::string auth_base_url;
        std::string sec_user_agent;
        bool ui_light_theme = false;
        bool ui_compact_mode = false;
    };

    struct SecretClearResult
    {
        Config config;
        std::string status;
    };

    Config ApplySettingsInput(Config config, const SettingsInput& input);
    SecretClearResult ClearStoredSecrets(Config config);
    std::vector<InfoItem> BuildSettingsHealthRows(const Config& config);
}
