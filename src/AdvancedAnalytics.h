#pragma once

#include "StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct Candle
    {
        std::string date;
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double close = 0.0;
        long long volume = 0;
    };

    struct IndicatorSnapshot
    {
        double sma20 = 0.0;
        double sma50 = 0.0;
        double ema12 = 0.0;
        double ema26 = 0.0;
        double rsi14 = 50.0;
        double macd = 0.0;
        double macd_signal = 0.0;
        double macd_histogram = 0.0;
        double bollinger_mid = 0.0;
        double bollinger_upper = 0.0;
        double bollinger_lower = 0.0;
        double atr14 = 0.0;
        double drawdown = 0.0;
        double relative_spy = 0.0;
        double relative_qqq = 0.0;
        double volatility = 0.0;
        std::string regime = "Neutral";
    };

    struct BacktestResult
    {
        int trades = 0;
        int wins = 0;
        int losses = 0;
        double win_rate = 0.0;
        double average_return = 0.0;
        double max_drawdown = 0.0;
        double profit_factor = 0.0;
        double best_trade = 0.0;
        double worst_trade = 0.0;
        std::vector<InfoItem> rows;
    };

    struct FundamentalSnapshot
    {
        std::string symbol;
        double revenue = 0.0;
        double eps = 0.0;
        double gross_margin = 0.0;
        double net_margin = 0.0;
        double debt_to_equity = 0.0;
        double free_cash_flow = 0.0;
        double pe = 0.0;
        double peg = 0.0;
        double dividend_yield = 0.0;
        std::string summary;
    };

    struct FilingItem
    {
        std::string form;
        std::string date;
        std::string title;
        std::string url;
        std::string summary;
        std::string risk_change;
    };

    struct NewsItem
    {
        std::string date;
        std::string source;
        std::string title;
        std::string sentiment;
        std::string summary;
        std::string url;
    };

    struct EarningsItem
    {
        std::string date;
        std::string status;
        double eps_estimate = 0.0;
        double eps_actual = 0.0;
        double surprise = 0.0;
        double estimated_move = 0.0;
        double drift = 0.0;
    };

    struct OptionSnapshot
    {
        double implied_volatility = 0.0;
        double expected_move = 0.0;
        double put_call_ratio = 0.0;
        double skew = 0.0;
        std::string unusual_flow;
    };

    struct ProviderStatus
    {
        std::string name;
        std::string status;
        std::string detail;
        std::string url;
        bool enabled = false;
    };

    struct MacroItem
    {
        std::string name;
        std::string value;
        std::string trend;
        std::string detail;
    };

    struct SymbolNote
    {
        std::string symbol;
        std::string tags;
        std::string note;
        std::string updated_at;
    };

    struct JournalEntry
    {
        std::string time;
        std::string symbol;
        std::string action;
        std::string reason;
        std::string exit_reason;
        std::string tags;
        std::string mistakes;
        std::string grade;
        double realized_pnl = 0.0;
    };

    struct WatchlistGroup
    {
        std::string name;
        std::vector<std::string> symbols;
    };

    struct ScreenPreset
    {
        std::string name;
        std::string description;
        std::string filter_key;
    };

    struct FocusItem
    {
        std::string symbol;
        std::string reason;
        std::string priority;
        std::string detail;
    };

    struct EtfExposure
    {
        std::string etf;
        double weight = 0.0;
        std::string note;
    };

    struct RiskSummary
    {
        double beta_exposure = 0.0;
        double estimated_drawdown = 0.0;
        double cash_drag = 0.0;
        double concentration = 0.0;
        std::vector<InfoItem> rows;
    };

    struct HistoricalCandlesResult
    {
        bool ok = false;
        bool live = false;
        bool cached = false;
        std::string status;
        std::string source;
        std::string fetched_at;
        std::string cache_age_label;
        std::string fallback_reason;
        std::vector<Candle> candles;
    };

    struct ResearchBundleResult
    {
        bool ok = false;
        bool live = false;
        bool cached = false;
        std::string status;
        std::string source;
        std::string fetched_at;
        std::string cache_age_label;
        std::string fallback_reason;
        FundamentalSnapshot fundamentals;
        std::vector<FilingItem> filings;
        std::vector<NewsItem> news;
        std::vector<EarningsItem> earnings;
    };

    std::vector<Candle> BuildSyntheticCandles(const StockQuote& quote, int days);
    HistoricalCandlesResult LoadHistoricalCandles(const Config& config, const StockQuote& quote, int days);
    ResearchBundleResult LoadResearchBundle(const Config& config, const StockQuote& quote);
    IndicatorSnapshot BuildIndicators(const std::vector<Candle>& candles, const StockQuote* spy, const StockQuote* qqq);
    BacktestResult RunSignalBacktest(const std::vector<Candle>& candles, const std::string& preset);
    FundamentalSnapshot BuildFundamentals(const StockQuote& quote);
    std::vector<FilingItem> BuildFilings(const std::string& symbol);
    std::vector<NewsItem> BuildNews(const std::string& symbol);
    std::vector<EarningsItem> BuildEarnings(const std::string& symbol);
    OptionSnapshot BuildOptionSnapshot(const StockQuote& quote);
    std::vector<ProviderStatus> BuildProviderStatuses(const Config& config);
    std::vector<MacroItem> BuildMacroDashboard();
    std::vector<WatchlistGroup> BuildWatchlistGroups(const std::vector<std::string>& symbols);
    std::vector<ScreenPreset> BuildScreenPresets();
    std::vector<FocusItem> BuildFocusItems(const StockState& state, const std::vector<PriceAlert>& alerts);
    std::vector<EtfExposure> BuildEtfExposure(const std::string& symbol);
    RiskSummary BuildRiskSummary(const StockState& state, const std::vector<PortfolioHolding>& holdings, double cash);
    std::vector<InfoItem> BuildTradeChecklist(const StockQuote& quote, const StockSignal& signal, const IndicatorSnapshot& indicators);
    std::vector<InfoItem> BuildModelExplanation(const StockQuote& quote, const StockSignal& signal, const IndicatorSnapshot& indicators);
    std::vector<InfoItem> BuildRiskCritic(const StockQuote& quote, const StockSignal& signal, const IndicatorSnapshot& indicators);
    std::vector<InfoItem> BuildSimilarSetups(const StockQuote& quote, const IndicatorSnapshot& indicators);
    std::vector<InfoItem> BuildModelAudit(const std::vector<TradePlan>& plans, const StockState& state);

    std::vector<SymbolNote> LoadSymbolNotes();
    bool SaveSymbolNotes(const std::vector<SymbolNote>& notes);
    std::vector<JournalEntry> LoadJournalEntries();
    bool SaveJournalEntries(const std::vector<JournalEntry>& entries);
}
