#include "PortfolioEngine.h"

namespace aegis
{
    namespace
    {
        InfoItem Row(const std::string& name, const std::string& label, const std::string& value, const std::string& detail)
        {
            InfoItem item;
            item.name = name;
            item.label = label;
            item.value = value;
            item.detail = detail;
            item.state = "Ready";
            item.tag = "portfolio-engine";
            return item;
        }
    }

    PortfolioEngineSnapshot BuildPortfolioEngineSnapshot(const StockState& state, const std::vector<PortfolioHolding>& holdings, double cash)
    {
        PortfolioEngineSnapshot snapshot;
        snapshot.cash = cash;
        for (const PortfolioHolding& holding : holdings)
        {
            const StockQuote* quote = FindQuote(state, holding.symbol);
            const double price = quote != nullptr && quote->price > 0.0 ? quote->price : holding.average_cost;
            snapshot.market_value += holding.shares * price;
            snapshot.cost_basis += holding.shares * holding.average_cost;
        }
        snapshot.unrealized_pnl = snapshot.market_value - snapshot.cost_basis;
        const double equity = snapshot.market_value + snapshot.cash;
        snapshot.rows = {
            Row("Paper portfolio", holdings.empty() ? "Empty" : "Tracked", std::to_string(static_cast<int>(holdings.size())) + " holdings", "PortfolioEngine centralizes cost basis, marked value, allocation, and journal sync boundaries."),
            Row("Market value", "Marked", FormatCurrency(snapshot.market_value), "Uses the current quote board with legacy cost-basis fallback."),
            Row("Unrealized P/L", snapshot.unrealized_pnl >= 0.0 ? "Positive" : "Negative", FormatCurrency(snapshot.unrealized_pnl), "Unrealized value is analytics context, not advice."),
            Row("Cash model", "Paper", FormatCurrency(snapshot.cash), "Paper cash remains separate from live brokerage balances."),
            Row("Equity model", "Paper", FormatCurrency(equity), "Future simulated fills and journal sync should write through PortfolioEngine.")
        };
        return snapshot;
    }

    std::vector<InfoItem> BuildPortfolioEngineRows(const StockState& state, const std::vector<PortfolioHolding>& holdings, double cash)
    {
        return BuildPortfolioEngineSnapshot(state, holdings, cash).rows;
    }
}
