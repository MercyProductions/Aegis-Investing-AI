#include "RegimeEngine.h"

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
            item.tag = "regime-engine";
            return item;
        }
    }

    RegimeEngineSnapshot BuildRegimeEngineSnapshot(const StockState& state)
    {
        RegimeEngineSnapshot snapshot;
        double avg_change = 0.0;
        double avg_abs_change = 0.0;
        for (const StockQuote& quote : state.quotes)
        {
            avg_change += quote.change_percent;
            avg_abs_change += std::fabs(quote.change_percent);
        }
        if (!state.quotes.empty())
        {
            avg_change /= static_cast<double>(state.quotes.size());
            avg_abs_change /= static_cast<double>(state.quotes.size());
        }

        snapshot.momentum_regime = avg_change > 0.75 ? "Bullish" : avg_change < -0.75 ? "Bearish" : "Neutral";
        snapshot.volatility_regime = avg_abs_change > 2.5 ? "Volatile" : avg_abs_change > 1.2 ? "Active" : "Normal";
        snapshot.risk_regime = snapshot.volatility_regime == "Volatile" || snapshot.momentum_regime == "Bearish" ? "Risk-off" : snapshot.momentum_regime == "Bullish" ? "Risk-on" : "Balanced";
        snapshot.rows = {
            Row("Momentum regime", snapshot.momentum_regime, FormatPercent(avg_change), "Average current quote change across the loaded universe."),
            Row("Volatility regime", snapshot.volatility_regime, FormatPercent(avg_abs_change), "Average absolute move across the loaded universe.", snapshot.volatility_regime == "Volatile" ? "Review" : "Ready"),
            Row("Risk state", snapshot.risk_regime, state.market_status.empty() ? "Unknown session" : state.market_status, "RegimeEngine combines momentum, volatility, and session state for dashboard context."),
            Row("Macro overlay", "Placeholder", "Not wired", "FRED/macro providers can attach rates, inflation, and risk-on/risk-off overlays here.", "Review")
        };
        return snapshot;
    }

    std::vector<InfoItem> BuildRegimeEngineRows(const StockState& state)
    {
        return BuildRegimeEngineSnapshot(state).rows;
    }
}
