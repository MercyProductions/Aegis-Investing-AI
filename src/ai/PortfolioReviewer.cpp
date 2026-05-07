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
            item.tag = "ai-portfolio";
            return item;
        }
    }

    std::vector<InfoItem> BuildPortfolioReviewRows(const StockState& state, const std::vector<PortfolioHolding>& holdings)
    {
        double total_value = 0.0;
        double largest_value = 0.0;
        std::string largest_symbol = "--";
        for (const PortfolioHolding& holding : holdings)
        {
            const StockQuote* quote = FindQuote(state, holding.symbol);
            const double price = quote != nullptr && quote->price > 0.0 ? quote->price : holding.average_cost;
            const double value = holding.shares * price;
            total_value += value;
            if (value > largest_value)
            {
                largest_value = value;
                largest_symbol = holding.symbol;
            }
        }
        const double largest_percent = total_value > 0.0 ? largest_value / total_value * 100.0 : 0.0;
        return {
            Row("Portfolio scope", holdings.empty() ? "Empty" : "Tracked", std::to_string(static_cast<int>(holdings.size())) + " holdings", "Portfolio review focuses on exposure, concentration, and explainability before paper execution."),
            Row("Concentration", largest_percent > 25.0 ? "Elevated" : "Contained", largest_symbol + " " + FormatPercent(largest_percent), largest_percent > 25.0 ? "Sector and single-name concentration exceed preferred review thresholds." : "No single saved position dominates the current paper portfolio.", largest_percent > 25.0 ? "Review" : "Ready"),
            Row("Marked exposure", "Paper", FormatCurrency(total_value), "Values use current market data where available and cost-basis fallback otherwise."),
            Row("Review note", "Research-first", "No advice", "Portfolio narrative should surface conditions and trade-offs without prescribing action.")
        };
    }
}
