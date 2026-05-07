#include "IntelligenceMemory.h"

#include "../../engine/RiskEngine.h"

#include <map>

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
            item.tag = "portfolio-memory";
            return item;
        }
    }

    std::vector<InfoItem> BuildPortfolioMemoryRows(const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<JournalEntry>& journal)
    {
        double exposure = 0.0;
        std::map<std::string, int> sector_counts;
        for (const PortfolioHolding& holding : holdings)
        {
            const StockQuote* quote = FindQuote(state, holding.symbol);
            const double price = quote != nullptr && quote->price > 0.0 ? quote->price : holding.average_cost;
            exposure += holding.shares * price;
            sector_counts[quote != nullptr && !quote->sector.empty() ? quote->sector : "Unclassified"]++;
        }
        int mistake_count = 0;
        for (const JournalEntry& entry : journal)
        {
            if (!entry.mistakes.empty())
                ++mistake_count;
        }
        std::string top_sector = "Unclassified";
        int top_count = 0;
        for (const auto& [sector, count] : sector_counts)
        {
            if (count > top_count)
            {
                top_sector = sector;
                top_count = count;
            }
        }
        return {
            Row("Portfolio memory", holdings.empty() ? "Empty" : "Tracked", std::to_string(static_cast<int>(holdings.size())), "Long-term memory compares current exposure to prior portfolio states."),
            Row("Exposure baseline", "Paper", FormatCurrency(exposure), "Current exposure becomes a future comparison point for AI-assisted portfolio reviews."),
            Row("Sector pattern", top_sector, std::to_string(top_count), "Recurring sector concentration should be remembered across sessions.", top_count > 2 ? "Review" : "Ready"),
            Row("Behavioral pattern", mistake_count > 0 ? "Review" : "Clear", std::to_string(mistake_count), "Repeated journal mistakes should influence future review prompts.", mistake_count > 0 ? "Review" : "Ready")
        };
    }
}
