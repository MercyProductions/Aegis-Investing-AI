#pragma once

#include "../AdvancedAnalytics.h"

#include <vector>

namespace aegis
{
    std::vector<InfoItem> BuildResearchNarrativeRows(const StockQuote& quote, const StockSignal& signal, const IndicatorSnapshot& indicators);
    std::vector<InfoItem> BuildMarketNarrativeRows(const StockState& state);
    std::vector<InfoItem> BuildPortfolioReviewRows(const StockState& state, const std::vector<PortfolioHolding>& holdings);
    std::vector<InfoItem> BuildRiskNarrativeRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings);
    std::vector<InfoItem> BuildWatchlistAnalystRows(const StockState& state);
}
