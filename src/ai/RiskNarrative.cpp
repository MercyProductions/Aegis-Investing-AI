#include "ResearchNarrative.h"

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
            item.tag = "ai-risk";
            return item;
        }
    }

    std::vector<InfoItem> BuildRiskNarrativeRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings)
    {
        int liquidity_warnings = 0;
        for (const PortfolioHolding& holding : holdings)
        {
            const StockQuote* quote = FindQuote(state, holding.symbol);
            if (quote != nullptr && quote->volume > 0 && quote->volume < 500000)
                ++liquidity_warnings;
        }

        return {
            Row("Safety posture", config.paper_only_mode && config.require_manual_confirmation ? "Guarded" : "Needs review", config.paper_only_mode ? "Paper" : "Unsafe", "Paper-only mode and manual confirmation should remain visible in every execution-like workflow.", config.paper_only_mode && config.require_manual_confirmation ? "Ready" : "Review"),
            Row("Liquidity risk", liquidity_warnings == 0 ? "Clear" : "Review", std::to_string(liquidity_warnings), "Low-volume holdings can produce unrealistic fills and need conservative simulation.", liquidity_warnings == 0 ? "Ready" : "Review"),
            Row("Allocation cap", "Configured", FormatCurrency(config.max_position_amount), "Maximum position size creates a first-line allocation boundary."),
            Row("Risk language", "Disciplined", "Required", "Narratives should describe uncertainty, volatility, and concentration without implying certainty or guaranteed profit.")
        };
    }
}
