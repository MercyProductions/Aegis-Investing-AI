#include "WorkspaceProfiles.h"

namespace aegis
{
    std::vector<WorkspaceProfile> BuildDefaultWorkspaceProfiles()
    {
        return {
            { "Growth", "Large-cap growth and trend persistence", "Dashboard + Chart Lab + Watchlist", "Momentum", "Moderate concentration" },
            { "Swing", "Shorter horizon setups with strict risk review", "Scanner + Chart Lab + Journal", "Breakout", "Tight stop discipline" },
            { "Long-term", "Portfolio allocation and fundamentals", "Portfolio + Research + Reports", "Quality", "Diversification first" },
            { "ETF", "Index, sector, and overlap analysis", "Compare + Portfolio + Risk", "Relative strength", "Exposure balance" },
            { "Earnings", "Event risk, revisions, and volatility", "Research + Chart Lab + Reports", "Earnings drift", "Reduced sizing" },
            { "Options Research", "Volatility and scenario review", "Chart Lab + Risk + Journal", "Volatility", "Paper-only scenarios" }
        };
    }

    std::vector<InfoItem> BuildWorkspaceProfileRows()
    {
        std::vector<InfoItem> rows;
        for (const WorkspaceProfile& profile : BuildDefaultWorkspaceProfiles())
        {
            InfoItem item;
            item.name = profile.name;
            item.label = profile.risk_profile;
            item.value = profile.scanner_preset;
            item.detail = profile.focus + ". Layout seed: " + profile.layout + ".";
            item.state = "Planned";
            item.tag = "workspace";
            rows.push_back(item);
        }
        return rows;
    }
}
