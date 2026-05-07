#include "ResearchNarrative.h"

#include <algorithm>

namespace aegis
{
    namespace
    {
        InfoItem Row(const std::string& name, const std::string& label, const std::string& value, const std::string& detail, const std::string& state = "Ready")
        {
            InfoItem item;
            item.name = name;
            item.label = label;
            item.value = value;
            item.detail = detail;
            item.state = state;
            item.tag = "ai-watchlist";
            return item;
        }
    }

    std::vector<InfoItem> BuildWatchlistAnalystRows(const StockState& state)
    {
        std::vector<StockSignal> ranked = state.signals;
        std::sort(ranked.begin(), ranked.end(), [](const StockSignal& left, const StockSignal& right) {
            return left.score > right.score;
        });

        const std::string top = ranked.empty() ? "--" : ranked.front().symbol;
        const std::string top_detail = ranked.empty()
            ? "No scanner output is currently loaded."
            : ranked.front().thesis;
        int review_count = 0;
        for (const StockSignal& signal : ranked)
        {
            if (signal.score < 50 || signal.risk.find("High") != std::string::npos)
                ++review_count;
        }

        return {
            Row("Watchlist triage", ranked.empty() ? "Empty" : "Ranked", std::to_string(static_cast<int>(ranked.size())) + " signals", "Watchlist analysis ranks research candidates without converting them into instructions."),
            Row("Top research candidate", top, ranked.empty() ? "--" : std::to_string(ranked.front().score), top_detail, ranked.empty() ? "Review" : "Ready"),
            Row("Needs review", review_count == 0 ? "Clear" : "Caution", std::to_string(review_count), "Low-score or high-risk names should be reviewed before any paper plan.", review_count == 0 ? "Ready" : "Review"),
            Row("Pinned workflow", "Planned", "Profiles", "Workspace profiles will store watchlists, scanner presets, chart overlays, and risk settings.")
        };
    }
}
