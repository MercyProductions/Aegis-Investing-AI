#include "RiskEngine.h"

#include "../Platform.h"

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
            item.tag = "risk-engine";
            return item;
        }
    }

    RiskEngineSnapshot BuildRiskEngineSnapshot(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings)
    {
        RiskEngineSnapshot snapshot;
        double largest_value = 0.0;
        for (const PortfolioHolding& holding : holdings)
        {
            const StockQuote* quote = FindQuote(state, holding.symbol);
            const double price = quote != nullptr && quote->price > 0.0 ? quote->price : holding.average_cost;
            const double value = holding.shares * price;
            snapshot.gross_exposure += value;
            largest_value = std::max(largest_value, value);
            if (quote != nullptr && quote->volume > 0 && quote->volume < 500000)
                ++snapshot.liquidity_warnings;
        }
        snapshot.largest_position_percent = snapshot.gross_exposure > 0.0 ? (largest_value / snapshot.gross_exposure) * 100.0 : 0.0;
        if (snapshot.largest_position_percent > 25.0)
            ++snapshot.concentration_warnings;

        const bool guardrails = config.paper_only_mode && config.require_manual_confirmation;
        snapshot.rows = {
            Row("Gross exposure", holdings.empty() ? "Empty" : "Tracked", FormatCurrency(snapshot.gross_exposure), "Current marked exposure across saved holdings."),
            Row("Largest position", snapshot.largest_position_percent > 25.0 ? "Concentrated" : "Balanced", FormatPercent(snapshot.largest_position_percent), "Concentration guardrail flags positions above preferred thresholds.", snapshot.largest_position_percent > 25.0 ? "Review" : "Ready"),
            Row("Liquidity warnings", snapshot.liquidity_warnings == 0 ? "Clear" : "Review", std::to_string(snapshot.liquidity_warnings), "Low-volume holdings should receive manual order review before paper fills.", snapshot.liquidity_warnings == 0 ? "Ready" : "Review"),
            Row("Paper guardrails", guardrails ? "Locked" : "Review", guardrails ? "Paper + manual" : "Needs review", "RiskEngine assumes paper-only mode and manual confirmation remain active.", guardrails ? "Ready" : "Review"),
            Row("Allocation budget", "Configured", FormatCurrency(config.max_position_amount), "Max position amount is the first-line allocation guardrail.")
        };
        return snapshot;
    }

    std::vector<InfoItem> BuildRiskEngineRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings)
    {
        return BuildRiskEngineSnapshot(config, state, holdings).rows;
    }
}
