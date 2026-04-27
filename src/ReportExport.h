#pragma once

#include "AdvancedAnalytics.h"
#include "AlertEngine.h"
#include "StockData.h"

#include <filesystem>
#include <string>
#include <vector>

namespace aegis
{
    struct SymbolResearchReport
    {
        StockQuote quote;
        StockSignal signal;
        IndicatorSnapshot indicators;
        BacktestResult backtest;
        BacktestResult strategy_backtest;
        std::string strategy_rule;
        std::string strategy_status;
        FundamentalSnapshot fundamentals;
        std::vector<FilingItem> filings;
        std::vector<NewsItem> news;
        std::vector<EarningsItem> earnings;
        std::vector<InfoItem> checklist;
        std::vector<InfoItem> explanation;
        std::vector<InfoItem> risk_critic;
        std::string history_source;
        std::string history_freshness;
        std::string history_fallback;
        std::string research_source;
        std::string research_freshness;
        std::string research_fallback;
    };

    struct DailyBriefingReport
    {
        StockState state;
        std::vector<FocusItem> focus;
        std::vector<PriceAlert> alerts;
        std::vector<PortfolioHolding> holdings;
        std::vector<AlertEvent> alert_events;
        double cash = 0.0;
    };

    std::filesystem::path ReportsDirectory();
    bool ExportSymbolResearchReport(const SymbolResearchReport& report, std::filesystem::path& output_path, std::string& status);
    bool ExportDailyBriefingReport(const DailyBriefingReport& report, std::filesystem::path& output_path, std::string& status);
}
