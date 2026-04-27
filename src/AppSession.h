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
        std::string strategy_rule;
    };

    std::filesystem::path AppSessionFile();
    AppSessionState LoadAppSessionState();
    AppSessionState LoadAppSessionState(const std::filesystem::path& path);
    bool SaveAppSessionState(const AppSessionState& session);
    bool SaveAppSessionState(const AppSessionState& session, const std::filesystem::path& path);
}
