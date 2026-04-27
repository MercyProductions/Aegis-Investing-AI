#pragma once

#include <filesystem>
#include <string>

namespace aegis
{
    struct AppSessionState
    {
        bool loaded = false;
        int selected_tab = 0;
        std::string selected_symbol;
        std::string compare_symbols;
        int chart_days = 180;
        int chart_aggregation = 0;
        bool show_chart_volume = true;
        bool show_trend_indicators = true;
        bool show_momentum_indicators = true;
        bool show_volatility_indicators = true;
        bool show_relative_indicators = true;
        std::string strategy_rule;
    };

    std::filesystem::path AppSessionFile();
    AppSessionState LoadAppSessionState();
    AppSessionState LoadAppSessionState(const std::filesystem::path& path);
    bool SaveAppSessionState(const AppSessionState& session);
    bool SaveAppSessionState(const AppSessionState& session, const std::filesystem::path& path);
}
