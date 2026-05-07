#pragma once

#include "../AdvancedAnalytics.h"
#include "../AlertEngine.h"

#include <string>
#include <vector>

namespace aegis
{
    enum class DailyFocusMode
    {
        PortfolioMonitor,
        WatchlistScanner,
        EarningsMode,
        VolatilityMode,
        PaperExecutionMode,
        ResearchMode
    };

    enum class PortfolioRiskState
    {
        Healthy,
        Caution,
        Elevated,
        Defensive,
        Critical
    };

    const char* DailyFocusModeName(DailyFocusMode mode);
    const char* PortfolioRiskStateName(PortfolioRiskState state);
    PortfolioRiskState ClassifyPortfolioRiskState(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<PriceAlert>& alerts);

    std::vector<InfoItem> BuildMorningBriefingRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<PriceAlert>& alerts, const std::vector<AlertEvent>& events, const std::vector<TradePlan>& plans);
    std::vector<InfoItem> BuildMarketCommandCenterRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<PriceAlert>& alerts, const std::vector<AlertEvent>& events, DailyFocusMode mode);
    std::vector<InfoItem> BuildEndOfDayReviewRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<AlertEvent>& events, const std::vector<TradePlan>& plans, const std::vector<JournalEntry>& journal);
    std::vector<InfoItem> BuildWeeklyReviewRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<TradePlan>& plans, const std::vector<JournalEntry>& journal);
    std::vector<InfoItem> BuildStrategyLabWorkflowRows(const Config& config, const StockState& state, const std::vector<TradePlan>& plans, const std::vector<Candle>& candles);
    std::vector<InfoItem> BuildWatchlistIntelligenceRows(const StockState& state, const std::vector<PriceAlert>& alerts, const std::vector<SymbolNote>& notes);
    std::vector<InfoItem> BuildPortfolioRiskConsoleRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<PriceAlert>& alerts);
    std::vector<InfoItem> BuildSymbolResearchPageRows(const Config& config, const StockState& state, const std::string& symbol, const std::vector<PortfolioHolding>& holdings, const std::vector<PriceAlert>& alerts, const std::vector<TradePlan>& plans, const std::vector<SymbolNote>& notes, const std::vector<JournalEntry>& journal);
    std::vector<InfoItem> BuildAiExplanationLayerRows(const StockState& state, const std::string& symbol);
    std::vector<InfoItem> BuildNotificationInboxRows(const Config& config, const StockState& state, const std::vector<AlertEvent>& events);
    std::vector<InfoItem> BuildCommandPaletteRows();
    std::vector<InfoItem> BuildSavedLayoutRows();
    std::vector<InfoItem> BuildImportExportWorkflowRows();
    std::vector<InfoItem> BuildDataQualityScoreRows(const StockState& state, const std::string& symbol = {});
    std::vector<InfoItem> BuildOnboardingWorkflowRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings);
}
