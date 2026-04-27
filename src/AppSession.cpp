#include "AppSession.h"

#include "Platform.h"
#include "StockData.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace aegis
{
    namespace
    {
        constexpr int kMinSessionTab = 0;
        constexpr int kMaxSessionTab = 9;
        constexpr int kMinChartDays = 30;
        constexpr int kMaxChartDays = 365;

        int ParseBoundedInt(const std::string& value, int fallback, int min_value, int max_value)
        {
            if (Trim(value).empty())
                return fallback;
            return std::clamp(std::atoi(value.c_str()), min_value, max_value);
        }
    }

    std::filesystem::path AppSessionFile()
    {
        return AppDataDirectory() / "app-session.tsv";
    }

    AppSessionState LoadAppSessionState()
    {
        return LoadAppSessionState(AppSessionFile());
    }

    AppSessionState LoadAppSessionState(const std::filesystem::path& path)
    {
        AppSessionState session;
        std::ifstream file(path);
        if (!file)
            return session;

        session.loaded = true;
        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            const size_t tab = line.find('\t');
            if (tab == std::string::npos)
                continue;

            const std::string key = Lower(Trim(line.substr(0, tab)));
            const std::string value = Trim(line.substr(tab + 1));

            if (key == "selected_tab")
                session.selected_tab = ParseBoundedInt(value, session.selected_tab, kMinSessionTab, kMaxSessionTab);
            else if (key == "selected_symbol" && !value.empty())
                session.selected_symbol = Upper(value);
            else if (key == "compare_symbols" && !value.empty())
                session.compare_symbols = JoinWatchlist(SplitWatchlist(value));
            else if (key == "chart_days")
                session.chart_days = ParseBoundedInt(value, session.chart_days, kMinChartDays, kMaxChartDays);
            else if (key == "strategy_rule" && !value.empty())
                session.strategy_rule = value;
        }

        return session;
    }

    bool SaveAppSessionState(const AppSessionState& session)
    {
        return SaveAppSessionState(session, AppSessionFile());
    }

    bool SaveAppSessionState(const AppSessionState& session, const std::filesystem::path& path)
    {
        std::error_code ec;
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream file(path, std::ios::trunc);
        if (!file)
            return false;

        file << "selected_tab\t" << std::clamp(session.selected_tab, kMinSessionTab, kMaxSessionTab) << '\n';
        file << "selected_symbol\t" << Upper(session.selected_symbol) << '\n';
        file << "compare_symbols\t" << JoinWatchlist(SplitWatchlist(session.compare_symbols)) << '\n';
        file << "chart_days\t" << std::clamp(session.chart_days, kMinChartDays, kMaxChartDays) << '\n';
        file << "strategy_rule\t" << Trim(session.strategy_rule) << '\n';
        return true;
    }
}
