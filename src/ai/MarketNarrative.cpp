#include "ResearchNarrative.h"

#include <cmath>

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
            item.tag = "ai-market";
            return item;
        }
    }

    std::vector<InfoItem> BuildMarketNarrativeRows(const StockState& state)
    {
        double average_change = 0.0;
        double average_abs_change = 0.0;
        for (const StockQuote& quote : state.quotes)
        {
            average_change += quote.change_percent;
            average_abs_change += std::fabs(quote.change_percent);
        }
        if (!state.quotes.empty())
        {
            average_change /= static_cast<double>(state.quotes.size());
            average_abs_change /= static_cast<double>(state.quotes.size());
        }

        return {
            Row("Market posture", average_change > 0.75 ? "Risk-on" : average_change < -0.75 ? "Risk-off" : "Balanced", FormatPercent(average_change), "Market narrative is derived from the loaded quote universe, not from a guarantee about future direction."),
            Row("Volatility tone", average_abs_change > 2.5 ? "Expanded" : "Normal", FormatPercent(average_abs_change), average_abs_change > 2.5 ? "Broader movement is elevated; allocation and stop assumptions need closer review." : "Average movement is within the current review band.", average_abs_change > 2.5 ? "Review" : "Ready"),
            Row("Session context", state.market_status.empty() ? "Unknown" : state.market_status, state.last_refresh_label.empty() ? "--" : state.last_refresh_label, state.market_detail.empty() ? "Market session metadata has not been refreshed." : state.market_detail),
            Row("Source note", state.source_badge.empty() ? "Unlabeled" : state.source_badge, state.loaded_from_api ? "Provider" : "Fallback", state.source_label.empty() ? "Source metadata should be made explicit before acting on results." : state.source_label, state.loaded_from_api ? "Ready" : "Review")
        };
    }
}
