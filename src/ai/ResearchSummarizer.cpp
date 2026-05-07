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
            item.tag = "ai-research";
            return item;
        }
    }

    std::vector<InfoItem> BuildResearchNarrativeRows(const StockQuote& quote, const StockSignal& signal, const IndicatorSnapshot& indicators)
    {
        const std::string momentum = quote.change_percent > 1.0
            ? "Momentum remains positive, but follow-through should be checked against volume and volatility."
            : quote.change_percent < -1.0
                ? "Momentum is negative, so risk review should come before any paper execution plan."
                : "Momentum is mixed and does not justify a high-certainty read.";
        const std::string trend = indicators.sma20 > indicators.sma50
            ? "Short-term trend is above the intermediate trend."
            : "Short-term trend is below or near the intermediate trend.";
        const std::string volatility = indicators.atr14 > 0.0 && quote.price > 0.0 && indicators.atr14 / quote.price > 0.04
            ? "Volatility expansion increases downside and position-sizing risk."
            : "Volatility is within the current model's normal review band.";

        return {
            Row("Signal explanation", signal.rating, std::to_string(signal.score), signal.thesis.empty() ? "Signal exists because weighted scanner factors crossed the configured threshold." : signal.thesis),
            Row("Momentum read", quote.change_percent >= 0.0 ? "Positive" : "Negative", FormatPercent(quote.change_percent), momentum),
            Row("Trend condition", indicators.sma20 > indicators.sma50 ? "Constructive" : "Weakening", FormatCurrency(indicators.sma20) + " / " + FormatCurrency(indicators.sma50), trend),
            Row("Volatility condition", volatility.find("expansion") != std::string::npos ? "Elevated" : "Normal", FormatCurrency(indicators.atr14), volatility, volatility.find("expansion") != std::string::npos ? "Review" : "Ready"),
            Row("Research boundary", "Manual review", "Required", "This narrative explains conditions and risk context; it is not financial advice or a prediction.")
        };
    }
}
