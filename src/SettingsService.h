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
        std::string website_base_url;
        std::string sec_user_agent;
        bool ui_light_theme = false;
        bool ui_compact_mode = false;
        bool ui_high_contrast = false;
        int font_scale_percent = 100;
    };

    struct SecretClearResult
    {
        Config config;
        std::string status;
    };

    struct WatchlistLimitResult
    {
        int symbol_count = 0;
        int refresh_cap = 0;
        bool within_limit = true;
        std::string summary;
        std::string detail;
    };

    Config ApplySettingsInput(Config config, const SettingsInput& input);
    SecretClearResult ClearStoredSecrets(Config config);
    WatchlistLimitResult ValidateWatchlistLimits(const Config& config);
    std::vector<InfoItem> BuildSettingsHealthRows(const Config& config);
}
