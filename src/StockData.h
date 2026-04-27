#pragma once

#include "Platform.h"

#include <string>
#include <vector>

namespace aegis
{
    struct InfoItem
    {
        std::string name;
        std::string label;
        std::string value;
        std::string weight;
        std::string detail;
        std::string tag;
        std::string state;
        std::string source;
        std::string time;
    };

    struct StockQuote
    {
        std::string symbol;
        std::string name;
        std::string sector;
        double price = 0.0;
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double previous_close = 0.0;
        double change = 0.0;
        double change_percent = 0.0;
        long long volume = 0;
        double market_cap = 0.0;
        double pe_ratio = 0.0;
        double dividend_yield = 0.0;
        double beta = 1.0;
        std::string latest_trading_day;
        std::string timestamp;
        std::string source;
        std::string status;
        std::string note;
        bool live = false;
        std::vector<float> history;
    };

    struct StockSignal
    {
        std::string symbol;
        std::string company;
        std::string rating;
        std::string posture;
        std::string horizon;
        int score = 50;
        int confidence = 50;
        double target_price = 0.0;
        double stop_level = 0.0;
        double position_budget = 0.0;
        std::string upside;
        std::string downside;
        std::string risk;
        std::string thesis;
        std::string source;
        std::vector<InfoItem> factors;
    };

    struct PortfolioHolding
    {
        std::string symbol;
        double shares = 0.0;
        double average_cost = 0.0;
        std::string note;
    };

    struct PriceAlert
    {
        std::string symbol;
        double trigger_price = 0.0;
        bool above = true;
        bool enabled = true;
        std::string note;
    };

    struct TradePlan
    {
        std::string created_at;
        std::string symbol;
        std::string rating;
        double entry = 0.0;
        double stop = 0.0;
        double target = 0.0;
        int shares = 0;
        double planned_risk = 0.0;
        double planned_reward = 0.0;
        std::string thesis;
        std::string status = "Open";
    };

    struct StockState
    {
        bool loaded_from_api = false;
        std::string tier = "free";
        std::string source_badge = "Demo market lab";
        std::string source_label = "Built-in sample watchlist. Add an Alpha Vantage key in Settings for direct quote refresh.";
        std::string market_status = "Unknown";
        std::string market_detail = "Market status has not been checked yet.";
        std::string last_refresh_label = "--";
        std::vector<StockQuote> quotes;
        std::vector<StockSignal> signals;
        std::vector<InfoItem> metrics;
        std::vector<InfoItem> scanners;
        std::vector<InfoItem> portfolio;
        std::vector<InfoItem> alerts;
        std::vector<InfoItem> rules;
        std::vector<InfoItem> research;
        std::string insight_title = "Portfolio posture";
        std::string insight_copy = "Aegis scores momentum, range, liquidity, and risk limits so the board can organize research without pretending to guarantee outcomes.";
        std::string selected_symbol = "AAPL";
        std::string selected_signal = "Watch";
        std::vector<float> market_history = { 48, 51, 49, 55, 58, 57, 61, 64, 63 };
    };

    struct AlphaValidationResult
    {
        bool configured = false;
        bool ok = false;
        int status_code = 0;
        std::string status;
        std::string detail;
        std::string endpoint;
    };

    StockState MakeDemoStockState();
    StockState BuildNativeStockState(const Config& config);
    AlphaValidationResult ValidateAlphaVantageKey(const std::string& api_key);
    std::vector<PortfolioHolding> LoadPortfolioHoldings();
    bool SavePortfolioHoldings(const std::vector<PortfolioHolding>& holdings);
    std::vector<PriceAlert> LoadPriceAlerts();
    bool SavePriceAlerts(const std::vector<PriceAlert>& alerts);
    std::vector<TradePlan> LoadTradePlans();
    bool SaveTradePlans(const std::vector<TradePlan>& plans);
    std::vector<std::string> SplitWatchlist(const std::string& value);
    std::string JoinWatchlist(const std::vector<std::string>& symbols);
    std::vector<const StockQuote*> FilterQuotes(const StockState& state, const std::string& filter, const std::string& search);
    std::vector<int> FilterSignalIndexes(const StockState& state, const std::string& filter, const std::string& search);
    const StockQuote* FindQuote(const StockState& state, const std::string& symbol);
    const StockSignal* FindSignal(const StockState& state, const std::string& symbol);
    std::string FormatCurrency(double value);
    std::string FormatCompactCurrency(double value);
    std::string FormatPercent(double value);
    std::string FormatVolume(long long value);
}
