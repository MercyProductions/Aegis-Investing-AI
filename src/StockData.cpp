#include "StockData.h"

#include "Diagnostics.h"
#include "Json.h"
#include "SymbolRules.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <set>
#include <sstream>

namespace aegis
{
    namespace
    {
        std::string ReadString(const JsonValue& object, const std::string& key, const std::string& fallback = "")
        {
            return object[key].AsString(fallback);
        }

        double ReadDouble(const JsonValue& object, const std::string& key, double fallback = 0.0)
        {
            return object[key].AsDouble(fallback);
        }

        long long ReadLongLong(const JsonValue& object, const std::string& key, long long fallback = 0)
        {
            const std::string raw = Trim(object[key].AsString());
            if (raw.empty())
                return fallback;
            try
            {
                return std::stoll(raw);
            }
            catch (...)
            {
                return fallback;
            }
        }

        int ClampInt(int value, int min_value, int max_value)
        {
            return std::max(min_value, std::min(max_value, value));
        }

        double ClampDouble(double value, double min_value, double max_value)
        {
            return std::max(min_value, std::min(max_value, value));
        }

        bool LooksProviderLimited(int http_status, const std::string& text)
        {
            if (http_status == 429)
                return true;
            const std::string lower = Lower(text);
            return lower.find("rate limit") != std::string::npos ||
                lower.find("api call frequency") != std::string::npos ||
                lower.find("standard api call frequency") != std::string::npos ||
                lower.find("premium endpoint") != std::string::npos ||
                lower.find("thank you for using alpha vantage") != std::string::npos;
        }

        bool ContainsWord(const std::string& text, const std::string& word)
        {
            return Lower(text).find(Lower(word)) != std::string::npos;
        }

        double ParsePercent(std::string value)
        {
            value.erase(std::remove(value.begin(), value.end(), '%'), value.end());
            value = Trim(value);
            if (value.empty())
                return 0.0;
            try
            {
                return std::stod(value);
            }
            catch (...)
            {
                return 0.0;
            }
        }

        std::string ConfiguredAlphaKey(const std::string& config_key)
        {
            const std::string saved = Trim(config_key);
            if (!saved.empty())
                return saved;
            return Trim(GetEnvUtf8(L"AEGIS_ALPHA_VANTAGE_API_KEY"));
        }

        std::filesystem::path HoldingsFile()
        {
            return AppDataDirectory() / "portfolio-holdings.tsv";
        }

        std::filesystem::path AlertsFile()
        {
            return AppDataDirectory() / "price-alerts.tsv";
        }

        std::filesystem::path TradePlansFile()
        {
            return AppDataDirectory() / "trade-plans.tsv";
        }

        std::string TsvField(std::string value)
        {
            for (char& c : value)
            {
                if (c == '\t' || c == '\r' || c == '\n')
                    c = ' ';
            }
            return Trim(value);
        }

        std::vector<std::string> SplitTabs(const std::string& line)
        {
            std::vector<std::string> parts;
            std::string part;
            std::stringstream stream(line);
            while (std::getline(stream, part, '\t'))
                parts.push_back(part);
            return parts;
        }

        double ParseDouble(const std::string& value, double fallback = 0.0)
        {
            try
            {
                return std::stod(Trim(value));
            }
            catch (...)
            {
                return fallback;
            }
        }

        std::string CompanyName(const std::string& symbol)
        {
            static const std::map<std::string, std::string> names = {
                {"AAPL", "Apple Inc."},
                {"MSFT", "Microsoft Corp."},
                {"NVDA", "NVIDIA Corp."},
                {"SPY", "SPDR S&P 500 ETF"},
                {"QQQ", "Invesco QQQ Trust"},
                {"TSLA", "Tesla Inc."},
                {"AMD", "Advanced Micro Devices"},
                {"GOOGL", "Alphabet Inc."},
                {"AMZN", "Amazon.com Inc."},
                {"META", "Meta Platforms"},
                {"AVGO", "Broadcom Inc."},
                {"JPM", "JPMorgan Chase"},
                {"V", "Visa Inc."},
                {"UNH", "UnitedHealth Group"},
                {"XOM", "Exxon Mobil"},
                {"COST", "Costco Wholesale"},
                {"NFLX", "Netflix Inc."}
            };
            const auto it = names.find(Upper(symbol));
            return it == names.end() ? Upper(symbol) + " Equity" : it->second;
        }

        std::string SectorName(const std::string& symbol)
        {
            static const std::map<std::string, std::string> sectors = {
                {"AAPL", "Technology"},
                {"MSFT", "Technology"},
                {"NVDA", "Semiconductors"},
                {"AMD", "Semiconductors"},
                {"GOOGL", "Communication Services"},
                {"META", "Communication Services"},
                {"AMZN", "Consumer Discretionary"},
                {"TSLA", "Consumer Discretionary"},
                {"SPY", "Broad Market ETF"},
                {"QQQ", "Growth ETF"},
                {"JPM", "Financials"},
                {"V", "Financials"},
                {"UNH", "Health Care"},
                {"XOM", "Energy"},
                {"COST", "Consumer Staples"},
                {"NFLX", "Communication Services"}
            };
            const auto it = sectors.find(Upper(symbol));
            return it == sectors.end() ? "Watchlist" : it->second;
        }

        double SeedValue(const std::string& symbol)
        {
            unsigned hash = 2166136261u;
            for (const char c : symbol)
            {
                hash ^= static_cast<unsigned char>(c);
                hash *= 16777619u;
            }
            return static_cast<double>(hash % 1000) / 1000.0;
        }

        std::vector<float> BuildHistory(const std::string& symbol, double price, double change_percent)
        {
            std::vector<float> points;
            points.reserve(32);
            const double seed = SeedValue(symbol);
            const double trend = ClampDouble(change_percent / 100.0, -0.06, 0.06);
            for (int i = 0; i < 32; ++i)
            {
                const double t = static_cast<double>(i) / 31.0;
                const double wave = std::sin((t * 6.28318 * 2.0) + seed * 5.0) * (0.014 + seed * 0.010);
                const double drift = (t - 0.5) * trend * 2.8;
                points.push_back(static_cast<float>(std::max(1.0, price * (1.0 + wave + drift))));
            }
            return points;
        }

        StockQuote DemoQuote(const std::string& symbol, double price, double change_percent, long long volume)
        {
            StockQuote quote;
            quote.symbol = Upper(symbol);
            quote.name = CompanyName(quote.symbol);
            quote.sector = SectorName(quote.symbol);
            quote.price = price;
            quote.previous_close = price / (1.0 + (change_percent / 100.0));
            quote.open = quote.previous_close * (1.0 + (change_percent * 0.28 / 100.0));
            quote.high = std::max(quote.price, quote.open) * (1.0 + 0.012 + SeedValue(symbol) * 0.018);
            quote.low = std::min(quote.price, quote.open) * (1.0 - 0.011 - SeedValue(symbol) * 0.014);
            quote.change = quote.price - quote.previous_close;
            quote.change_percent = change_percent;
            quote.volume = volume;
            quote.market_cap = quote.price * (800000000.0 + SeedValue(symbol) * 14000000000.0);
            quote.pe_ratio = 18.0 + SeedValue(symbol) * 44.0;
            quote.dividend_yield = (SeedValue(symbol) > 0.55 ? 0.4 + SeedValue(symbol) * 2.1 : 0.0);
            quote.beta = 0.75 + SeedValue(symbol) * 1.15;
            quote.market_cap_estimated = true;
            quote.beta_estimated = true;
            quote.fundamentals_estimated = true;
            quote.latest_trading_day = "Demo";
            quote.timestamp = NowTimeLabel();
            quote.source = "Demo";
            quote.status = "Research";
            quote.note = "Sample quote for layout, scoring, and workflow until a market-data key is saved. Fundamentals and beta are estimated.";
            quote.data_quality = "Demo quote / estimated metrics";
            quote.history = BuildHistory(quote.symbol, quote.price, quote.change_percent);
            return quote;
        }

        StockQuote ParseGlobalQuote(const std::string& symbol, const JsonValue& root)
        {
            StockQuote quote;
            quote.symbol = Upper(symbol);
            quote.name = CompanyName(quote.symbol);
            quote.sector = SectorName(quote.symbol);

            const JsonValue& global = root["Global Quote"];
            if (!global.IsObject())
            {
                quote.note = root["Note"].AsString(root["Information"].AsString("No quote payload returned."));
                return quote;
            }

            quote.symbol = Upper(ReadString(global, "01. symbol", symbol));
            quote.name = CompanyName(quote.symbol);
            quote.sector = SectorName(quote.symbol);
            quote.open = ReadDouble(global, "02. open", 0.0);
            quote.high = ReadDouble(global, "03. high", 0.0);
            quote.low = ReadDouble(global, "04. low", 0.0);
            quote.price = ReadDouble(global, "05. price", 0.0);
            quote.volume = ReadLongLong(global, "06. volume", 0);
            quote.latest_trading_day = ReadString(global, "07. latest trading day");
            quote.previous_close = ReadDouble(global, "08. previous close", 0.0);
            quote.change = ReadDouble(global, "09. change", quote.price - quote.previous_close);
            quote.change_percent = ParsePercent(ReadString(global, "10. change percent"));
            quote.timestamp = NowTimeLabel();
            quote.source = "Alpha Vantage";
            quote.status = quote.price > 0.0 ? "Synced" : "No price";
            quote.live = true;
            quote.market_cap = quote.price * (800000000.0 + SeedValue(quote.symbol) * 14000000000.0);
            quote.pe_ratio = 18.0 + SeedValue(quote.symbol) * 44.0;
            quote.dividend_yield = (SeedValue(quote.symbol) > 0.55 ? 0.4 + SeedValue(quote.symbol) * 2.1 : 0.0);
            quote.beta = 0.75 + SeedValue(quote.symbol) * 1.15;
            quote.market_cap_estimated = true;
            quote.beta_estimated = true;
            quote.fundamentals_estimated = true;
            quote.note = "Price, range, volume, and previous close are from Alpha Vantage. Market cap, P/E, dividend yield, and beta are estimated until a fundamentals provider syncs.";
            quote.data_quality = "Live quote / estimated metrics";
            quote.history = BuildHistory(quote.symbol, quote.price > 0.0 ? quote.price : 100.0, quote.change_percent);
            return quote;
        }

        int BuildScore(const StockQuote& quote)
        {
            const double intraday_range = quote.price > 0.0 ? ((quote.high - quote.low) / quote.price) * 100.0 : 0.0;
            int score = 50;
            score += static_cast<int>(std::round(quote.change_percent * 4.0));
            if (quote.price > quote.open && quote.open > 0.0)
                score += 5;
            if (quote.price > quote.previous_close && quote.previous_close > 0.0)
                score += 4;
            if (quote.volume > 25000000)
                score += 4;
            if (intraday_range > 4.5)
                score -= 6;
            if (quote.beta > 1.55)
                score -= 4;
            if (quote.symbol == "SPY" || quote.symbol == "QQQ")
                score += 2;
            return ClampInt(score, 15, 95);
        }

        std::string RatingForScore(int score)
        {
            if (score >= 76)
                return "Accumulate";
            if (score >= 64)
                return "Watch Buy";
            if (score >= 50)
                return "Hold";
            if (score >= 38)
                return "Wait";
            return "Trim Risk";
        }

        std::string RiskForQuote(const StockQuote& quote)
        {
            const double range = quote.price > 0.0 ? ((quote.high - quote.low) / quote.price) * 100.0 : 0.0;
            if (range > 5.0 || quote.beta > 1.75)
                return "High";
            if (range > 2.6 || quote.beta > 1.25)
                return "Medium";
            return "Lower";
        }

        StockSignal BuildSignal(const StockQuote& quote, const Config& config)
        {
            StockSignal signal;
            signal.symbol = quote.symbol;
            signal.company = quote.name;
            signal.score = BuildScore(quote);
            const int source_bonus = quote.source == "Alpha Vantage" && quote.live ? 5 : -3;
            const int confidence_penalty = DataConfidencePenalty(quote);
            signal.confidence = ClampInt(signal.score + source_bonus - confidence_penalty, 10, 96);
            signal.rating = RatingForScore(signal.score);
            signal.posture = signal.score >= config.min_conviction ? "Eligible watch" : "Below threshold";
            signal.horizon = signal.score >= 70 ? "2-8 weeks" : "1-4 weeks";
            signal.target_price = quote.price * (1.0 + ClampDouble((signal.score - 48) / 100.0, 0.02, 0.18));
            signal.stop_level = quote.price * (1.0 - ClampDouble((100 - signal.score) / 300.0, 0.04, 0.16));
            signal.position_budget = std::min(config.max_position_amount, config.portfolio_cash * (config.max_portfolio_risk_percent / 100.0) * 8.0);
            signal.upside = FormatPercent(quote.price > 0.0 ? ((signal.target_price - quote.price) / quote.price) * 100.0 : 0.0);
            signal.downside = FormatPercent(quote.price > 0.0 ? ((signal.stop_level - quote.price) / quote.price) * 100.0 : 0.0);
            signal.risk = RiskForQuote(quote);
            signal.source = quote.source;

            if (signal.score >= 76)
                signal.thesis = quote.symbol + " has positive price momentum with enough liquidity to stay on the high-conviction research board.";
            else if (signal.score >= 64)
                signal.thesis = quote.symbol + " is improving, but the model wants confirmation from follow-through and broader market tone.";
            else if (signal.score >= 50)
                signal.thesis = quote.symbol + " is balanced. Aegis keeps it visible without forcing a bullish setup.";
            else
                signal.thesis = quote.symbol + " is showing weak or noisy conditions, so risk control matters more than entry timing.";

            const double range = quote.price > 0.0 ? ((quote.high - quote.low) / quote.price) * 100.0 : 0.0;
            signal.factors = {
                {"Momentum", "", FormatPercent(quote.change_percent), std::to_string(signal.score), "Latest quote change compared with previous close.", "Price"},
                {"Intraday range", "", FormatPercent(range), "", "A wide range raises execution and stop-placement risk.", "Risk"},
                {"Liquidity", "", FormatVolume(quote.volume), "", "Higher volume improves confidence that prices are observable and tradable.", "Volume"},
                {"Data confidence", "", std::to_string(signal.confidence) + "%", "-" + std::to_string(confidence_penalty), DataQualityBadge(quote) + ". " + quote.note, "Freshness"},
                {"Risk budget", "", FormatCurrency(signal.position_budget), "", "Position sizing is capped by the configured paper portfolio controls.", "Sizing"}
            };
            return signal;
        }

        std::string SearchHaystack(const StockQuote& quote)
        {
            return Lower(quote.symbol + " " + quote.name + " " + quote.sector + " " + quote.status + " " + quote.source);
        }

        std::string SignalHaystack(const StockSignal& signal)
        {
            return Lower(signal.symbol + " " + signal.company + " " + signal.rating + " " + signal.posture + " " + signal.risk + " " + signal.thesis);
        }

        bool ContainsTerms(const std::string& haystack, const std::string& search)
        {
            const std::string query = Lower(Trim(search));
            if (query.empty())
                return true;
            std::stringstream stream(query);
            std::string term;
            while (stream >> term)
            {
                if (haystack.find(term) == std::string::npos)
                    return false;
            }
            return true;
        }

        std::string MarketStatusFromAlpha(const JsonValue& root)
        {
            const JsonValue& markets = root["markets"];
            if (!markets.IsArray())
                return "Unknown";

            for (const JsonValue& market : markets.array_value)
            {
                const std::string type = Lower(ReadString(market, "market_type"));
                const std::string region = Lower(ReadString(market, "region"));
                if (type.find("equity") != std::string::npos && (region.find("united states") != std::string::npos || region == "us"))
                {
                    const std::string status = ReadString(market, "current_status", "Unknown");
                    return status.empty() ? "Unknown" : status;
                }
            }
            return "Unknown";
        }

        std::string MarketDetailFromAlpha(const JsonValue& root)
        {
            const JsonValue& markets = root["markets"];
            if (!markets.IsArray())
                return "Alpha Vantage market status returned without a US equity venue row.";

            for (const JsonValue& market : markets.array_value)
            {
                const std::string type = Lower(ReadString(market, "market_type"));
                const std::string region = Lower(ReadString(market, "region"));
                if (type.find("equity") != std::string::npos && (region.find("united states") != std::string::npos || region == "us"))
                {
                    std::string primary = ReadString(market, "primary_exchanges");
                    if (primary.empty())
                        primary = "US equities";
                    return primary + " status checked at " + NowTimeLabel() + ".";
                }
            }
            return "Market status checked, but no US equity venue row was selected.";
        }

        void SortSignals(std::vector<StockSignal>& signals)
        {
            std::sort(signals.begin(), signals.end(), [](const StockSignal& a, const StockSignal& b) {
                if (a.score != b.score)
                    return a.score > b.score;
                return a.symbol < b.symbol;
            });
        }

        void BuildDerivedState(StockState& state, const Config& config)
        {
            state.signals.clear();
            state.signals.reserve(state.quotes.size());
            for (const StockQuote& quote : state.quotes)
            {
                if (quote.price > 0.0)
                    state.signals.push_back(BuildSignal(quote, config));
            }
            SortSignals(state.signals);

            int positive = 0;
            int eligible = 0;
            int estimated_metric_count = 0;
            double change_sum = 0.0;
            long long total_volume = 0;
            for (const StockQuote& quote : state.quotes)
            {
                if (quote.change_percent >= 0.0)
                    ++positive;
                if (quote.market_cap_estimated || quote.beta_estimated || quote.fundamentals_estimated)
                    ++estimated_metric_count;
                change_sum += quote.change_percent;
                total_volume += quote.volume;
            }
            for (const StockSignal& signal : state.signals)
            {
                if (signal.confidence >= config.min_conviction)
                    ++eligible;
            }

            const double average_change = state.quotes.empty() ? 0.0 : change_sum / static_cast<double>(state.quotes.size());
            state.metrics = {
                {"Symbols", "", std::to_string(static_cast<int>(state.quotes.size())), "", "Current watchlist symbols loaded into the board."},
                {"Average move", "", FormatPercent(average_change), "", "Average percent change across the visible quote set."},
                {"Positive tape", "", std::to_string(positive), "", "Symbols trading above their previous close."},
                {"Eligible signals", "", std::to_string(eligible), "", "Signals at or above the configured conviction threshold."},
                {"Volume", "", FormatVolume(total_volume), "", "Combined latest reported volume across the loaded quotes."}
            };

            state.scanners.clear();
            for (const StockSignal& signal : state.signals)
            {
                state.scanners.push_back({
                    signal.symbol,
                    signal.rating,
                    std::to_string(signal.score),
                    signal.risk,
                    signal.thesis,
                    signal.posture,
                    signal.source
                });
            }

            double planned = 0.0;
            for (const StockSignal& signal : state.signals)
            {
                if (signal.confidence >= config.min_conviction)
                    planned += signal.position_budget;
            }
            state.portfolio = {
                {"Cash model", "", FormatCurrency(config.portfolio_cash), "", "Paper portfolio cash used for sizing calculations."},
                {"Max position", "", FormatCurrency(config.max_position_amount), "", "Upper cap for any single idea before risk controls."},
                {"Risk per idea", "", FormatPercent(config.max_portfolio_risk_percent), "", "Configured risk budget used by the sizing worksheet."},
                {"Planned exposure", "", FormatCurrency(planned), "", "Sum of eligible signal budgets before manual trade review."},
                {"Execution mode", "", config.paper_only_mode ? "Paper only" : "Manual live", "", config.require_manual_confirmation ? "Manual confirmation required." : "Manual confirmation can be disabled in Settings."}
            };

            state.rules = {
                {"No auto trading", "", "Active", "", "The desktop tool never places orders by itself.", "Safety"},
                {"Data freshness", "", state.source_badge, "", "Quotes are labeled by source and fallback status.", "Data"},
                {"Estimated metrics", "", estimated_metric_count > 0 ? std::to_string(estimated_metric_count) : "None", "", estimated_metric_count > 0 ? "Some market cap, beta, valuation, or fundamental values are estimated and confidence is decayed accordingly." : "Loaded quote metrics are provider-backed where currently used.", "Data"},
                {"Conviction floor", "", std::to_string(config.min_conviction), "", "Signals below this score remain research-only.", "Model"},
                {"Position cap", "", FormatCurrency(config.max_position_amount), "", "Every idea is clipped by the configured max position.", "Risk"}
            };

            state.alerts.clear();
            for (const StockQuote& quote : state.quotes)
            {
                if (std::fabs(quote.change_percent) >= 3.0)
                {
                    state.alerts.push_back({
                        quote.symbol,
                        quote.change_percent >= 0.0 ? "Upside move" : "Downside move",
                        FormatPercent(quote.change_percent),
                        "",
                        quote.name + " is moving enough to deserve manual news and liquidity review.",
                        quote.sector,
                        quote.source
                    });
                }
            }
            if (state.alerts.empty())
            {
                state.alerts.push_back({"Watchlist", "Calm tape", "No extremes", "", "No loaded symbol has crossed the default large-move alert band.", "Monitor", state.source_badge});
            }

            state.research = {
                {"Quote feed", "", state.source_badge, "", state.source_label, "Data"},
                {"Market status", "", state.market_status, "", state.market_detail, "Venue"},
                {"Data quality", "", estimated_metric_count > 0 ? "Mixed" : "Provider-backed", "", estimated_metric_count > 0 ? "Estimated fields are labeled and lower model confidence until a provider-backed value is available." : "No estimated quote metrics are flagged in the active board.", "Trust"},
                {"Model posture", "", std::to_string(static_cast<int>(state.signals.size())) + " ideas", "", "Scores blend price change, range, liquidity, and configured risk limits.", "Research"},
                {"Reminder", "", "Educational", "", "Outputs are research organization only, not investment advice or a promise of returns.", "Risk"}
            };

            if (!state.signals.empty())
            {
                const StockSignal& top = state.signals.front();
                state.selected_symbol = top.symbol;
                state.selected_signal = top.rating;
                state.insight_title = top.symbol + " " + top.rating;
                state.insight_copy = top.thesis;
                const StockQuote* quote = FindQuote(state, top.symbol);
                if (quote != nullptr)
                    state.market_history = quote->history;
            }
        }
    }

    std::vector<std::string> SplitWatchlist(const std::string& value)
    {
        std::vector<std::string> symbols;
        std::set<std::string> seen;
        for (const std::string& token : SplitRawSymbolTokens(value))
        {
            const SymbolValidationResult validated = ValidateTickerSymbol(token);
            if (!validated.ok || seen.find(validated.symbol) != seen.end())
                continue;
            seen.insert(validated.symbol);
            symbols.push_back(validated.symbol);
        }
        return symbols;
    }

    std::string JoinWatchlist(const std::vector<std::string>& symbols)
    {
        std::ostringstream stream;
        bool first = true;
        for (const std::string& symbol : symbols)
        {
            const std::string normalized = NormalizeTickerSymbol(symbol);
            if (normalized.empty())
                continue;
            if (!first)
                stream << ",";
            stream << normalized;
            first = false;
        }
        return stream.str();
    }

    StockState MakeDemoStockState()
    {
        Config config;
        StockState state;
        state.source_badge = "Demo market lab";
        state.source_label = "Sample data for the native stock investing workspace. Save an Alpha Vantage API key to refresh direct quote data.";
        state.market_status = "Demo";
        state.market_detail = "Demo mode keeps the board available without making network calls.";
        state.last_refresh_label = NowTimeLabel();
        state.quotes = {
            DemoQuote("AAPL", 196.21, 1.18, 48122000),
            DemoQuote("MSFT", 421.48, 0.72, 26888000),
            DemoQuote("NVDA", 911.30, 2.84, 62500400),
            DemoQuote("SPY", 514.72, 0.34, 76330200),
            DemoQuote("QQQ", 441.09, 0.56, 42118700),
            DemoQuote("TSLA", 178.22, -1.94, 98900400),
            DemoQuote("AMD", 157.63, 1.42, 58330100),
            DemoQuote("GOOGL", 153.77, -0.38, 23100200)
        };
        BuildDerivedState(state, config);
        state.loaded_from_api = false;
        return state;
    }

    StockState BuildNativeStockState(const Config& config)
    {
        const std::string key = ConfiguredAlphaKey(config.alpha_vantage_api_key);
        if (key.empty())
        {
            StockState demo = MakeDemoStockState();
            demo.source_badge = "Demo market lab";
            demo.source_label = "No Alpha Vantage key is saved, so Aegis is showing the built-in sample board.";
            return demo;
        }

        StockState state;
        state.loaded_from_api = true;
        state.tier = "direct";
        state.source_badge = "Alpha Vantage";
        state.source_label = "Direct quote refresh through Alpha Vantage GLOBAL_QUOTE. Some plans return end-of-day or delayed data.";
        state.last_refresh_label = NowTimeLabel();

        const std::string status_url = "https://www.alphavantage.co/query?function=MARKET_STATUS&apikey=" + UrlEncode(key);
        const HttpResponse status_response = HttpGet(status_url);
        if (status_response.error.empty() && status_response.status_code >= 200 && status_response.status_code < 300)
        {
            const JsonParseResult parsed = ParseJson(status_response.body);
            if (parsed.ok)
            {
                state.market_status = MarketStatusFromAlpha(parsed.value);
                state.market_detail = MarketDetailFromAlpha(parsed.value);
            }
        }
        else
        {
            state.market_status = "Unknown";
            state.market_detail = status_response.error.empty() ? "Market status endpoint returned HTTP " + std::to_string(status_response.status_code) + "." : status_response.error;
            AppendDiagnosticEvent({ "warning", "Alpha Vantage", "MARKET_STATUS", "", LooksProviderLimited(status_response.status_code, status_response.body + status_response.error) ? "rate-limit" : "fallback", state.market_detail, status_response.error, status_response.status_code, 0, false });
        }

        std::vector<std::string> symbols = SplitWatchlist(config.watchlist);
        if (symbols.empty())
            symbols = SplitWatchlist(Config{}.watchlist);
        if (static_cast<int>(symbols.size()) > config.max_symbols)
            symbols.resize(static_cast<size_t>(config.max_symbols));

        std::vector<std::string> warnings;
        for (const std::string& symbol : symbols)
        {
            const std::string url = "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=" + UrlEncode(symbol) + "&apikey=" + UrlEncode(key);
            const HttpResponse response = HttpGet(url);
            if (!response.error.empty())
            {
                warnings.push_back(symbol + ": " + response.error);
                AppendDiagnosticEvent({ "warning", "Alpha Vantage", "GLOBAL_QUOTE", symbol, "error", "Quote request failed.", response.error, response.status_code, 0, false });
                continue;
            }
            if (response.status_code < 200 || response.status_code >= 300)
            {
                warnings.push_back(symbol + ": HTTP " + std::to_string(response.status_code));
                AppendDiagnosticEvent({ "warning", "Alpha Vantage", "GLOBAL_QUOTE", symbol, LooksProviderLimited(response.status_code, response.body) ? "rate-limit" : "http-error", "Quote request returned HTTP " + std::to_string(response.status_code) + ".", "", response.status_code, 0, false });
                continue;
            }

            const JsonParseResult parsed = ParseJson(response.body);
            if (!parsed.ok)
            {
                warnings.push_back(symbol + ": quote parse failed");
                AppendDiagnosticEvent({ "warning", "Alpha Vantage", "GLOBAL_QUOTE", symbol, "parse-error", "Quote JSON parse failed.", parsed.error, response.status_code, 0, false });
                continue;
            }
            const std::string note = parsed.value["Note"].AsString(parsed.value["Information"].AsString());
            if (!note.empty())
            {
                warnings.push_back(symbol + ": " + note);
                AppendDiagnosticEvent({ "warning", "Alpha Vantage", "GLOBAL_QUOTE", symbol, LooksProviderLimited(response.status_code, note) ? "rate-limit" : "provider-note", "Quote provider returned a notice.", note, response.status_code, 0, false });
                break;
            }

            StockQuote quote = ParseGlobalQuote(symbol, parsed.value);
            if (quote.price <= 0.0)
            {
                warnings.push_back(symbol + ": no price returned");
                continue;
            }
            state.quotes.push_back(std::move(quote));
        }

        if (state.quotes.empty())
        {
            StockState demo = MakeDemoStockState();
            demo.loaded_from_api = false;
            demo.source_badge = "Demo fallback";
            demo.source_label = warnings.empty()
                ? "Direct quote refresh returned no usable symbols, so Aegis is showing demo data."
                : "Direct quote refresh paused: " + warnings.front();
            demo.market_status = state.market_status;
            demo.market_detail = state.market_detail;
            return demo;
        }

        BuildDerivedState(state, config);
        for (const std::string& warning : warnings)
        {
            state.alerts.push_back({"Data warning", "Quote skipped", "", "", warning, "Alpha Vantage", "Refresh"});
        }
        return state;
    }

    AlphaValidationResult ValidateAlphaVantageKey(const std::string& api_key)
    {
        AlphaValidationResult result;
        const std::string key = ConfiguredAlphaKey(api_key);
        result.configured = !key.empty();
        result.endpoint = "MARKET_STATUS";
        if (key.empty())
        {
            result.status = "No key saved";
            result.detail = "Paste an Alpha Vantage key and save settings before direct quote refresh.";
            return result;
        }

        const std::string url = "https://www.alphavantage.co/query?function=MARKET_STATUS&apikey=" + UrlEncode(key);
        const HttpResponse response = HttpGet(url);
        result.status_code = response.status_code;
        if (!response.error.empty())
        {
            result.status = "Network error";
            result.detail = response.error;
            return result;
        }
        if (response.status_code < 200 || response.status_code >= 300)
        {
            result.status = "HTTP " + std::to_string(response.status_code);
            result.detail = "Alpha Vantage returned a non-success response.";
            return result;
        }

        const JsonParseResult parsed = ParseJson(response.body);
        if (!parsed.ok)
        {
            result.status = "Parse error";
            result.detail = parsed.error;
            return result;
        }

        const std::string note = parsed.value["Note"].AsString(parsed.value["Information"].AsString(parsed.value["Error Message"].AsString()));
        if (!note.empty())
        {
            result.status = "Limited";
            result.detail = note;
            return result;
        }

        result.ok = true;
        result.status = "Connected";
        result.detail = "Key reached Alpha Vantage and market status responded.";
        return result;
    }

    std::vector<PortfolioHolding> LoadPortfolioHoldings()
    {
        return LoadPortfolioHoldings(HoldingsFile());
    }

    std::vector<PortfolioHolding> LoadPortfolioHoldings(const std::filesystem::path& path)
    {
        std::vector<PortfolioHolding> holdings;
        std::ifstream file(path);
        if (!file)
            return holdings;

        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (Trim(line).empty())
                continue;

            const std::vector<std::string> parts = SplitTabs(line);
            if (parts.size() < 3)
                continue;

            PortfolioHolding holding;
            holding.symbol = Upper(Trim(parts[0]));
            holding.shares = ParseDouble(parts[1]);
            holding.average_cost = ParseDouble(parts[2]);
            if (parts.size() >= 4)
                holding.note = Trim(parts[3]);
            if (!holding.symbol.empty() && holding.shares > 0.0 && holding.average_cost >= 0.0)
                holdings.push_back(holding);
        }

        std::sort(holdings.begin(), holdings.end(), [](const PortfolioHolding& a, const PortfolioHolding& b) {
            return a.symbol < b.symbol;
        });
        return holdings;
    }

    bool SavePortfolioHoldings(const std::vector<PortfolioHolding>& holdings)
    {
        return SavePortfolioHoldings(holdings, HoldingsFile());
    }

    bool SavePortfolioHoldings(const std::vector<PortfolioHolding>& holdings, const std::filesystem::path& path)
    {
        std::error_code ec;
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::trunc);
        if (!file)
            return false;

        for (const PortfolioHolding& holding : holdings)
        {
            if (Trim(holding.symbol).empty() || holding.shares <= 0.0)
                continue;
            file << TsvField(Upper(holding.symbol)) << '\t'
                 << holding.shares << '\t'
                 << holding.average_cost << '\t'
                 << TsvField(holding.note) << '\n';
        }
        return true;
    }

    std::vector<PriceAlert> LoadPriceAlerts()
    {
        return LoadPriceAlerts(AlertsFile());
    }

    std::vector<PriceAlert> LoadPriceAlerts(const std::filesystem::path& path)
    {
        std::vector<PriceAlert> alerts;
        std::ifstream file(path);
        if (!file)
            return alerts;

        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (Trim(line).empty())
                continue;

            const std::vector<std::string> parts = SplitTabs(line);
            if (parts.size() < 4)
                continue;

            PriceAlert alert;
            alert.symbol = Upper(Trim(parts[0]));
            alert.trigger_price = ParseDouble(parts[1]);
            alert.above = Lower(Trim(parts[2])) != "below";
            alert.enabled = Lower(Trim(parts[3])) != "false" && Trim(parts[3]) != "0";
            if (parts.size() >= 5)
                alert.note = Trim(parts[4]);
            if (!alert.symbol.empty() && alert.trigger_price > 0.0)
                alerts.push_back(alert);
        }

        std::sort(alerts.begin(), alerts.end(), [](const PriceAlert& a, const PriceAlert& b) {
            if (a.symbol != b.symbol)
                return a.symbol < b.symbol;
            return a.trigger_price < b.trigger_price;
        });
        return alerts;
    }

    bool SavePriceAlerts(const std::vector<PriceAlert>& alerts)
    {
        return SavePriceAlerts(alerts, AlertsFile());
    }

    bool SavePriceAlerts(const std::vector<PriceAlert>& alerts, const std::filesystem::path& path)
    {
        std::error_code ec;
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::trunc);
        if (!file)
            return false;

        for (const PriceAlert& alert : alerts)
        {
            if (Trim(alert.symbol).empty() || alert.trigger_price <= 0.0)
                continue;
            file << TsvField(Upper(alert.symbol)) << '\t'
                 << alert.trigger_price << '\t'
                 << (alert.above ? "above" : "below") << '\t'
                 << (alert.enabled ? "true" : "false") << '\t'
                 << TsvField(alert.note) << '\n';
        }
        return true;
    }

    std::vector<TradePlan> LoadTradePlans()
    {
        return LoadTradePlans(TradePlansFile());
    }

    std::vector<TradePlan> LoadTradePlans(const std::filesystem::path& path)
    {
        std::vector<TradePlan> plans;
        std::ifstream file(path);
        if (!file)
            return plans;

        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (Trim(line).empty())
                continue;

            const std::vector<std::string> parts = SplitTabs(line);
            if (parts.size() < 10)
                continue;

            TradePlan plan;
            plan.created_at = Trim(parts[0]);
            plan.symbol = Upper(Trim(parts[1]));
            plan.rating = Trim(parts[2]);
            plan.entry = ParseDouble(parts[3]);
            plan.stop = ParseDouble(parts[4]);
            plan.target = ParseDouble(parts[5]);
            plan.shares = static_cast<int>(std::round(ParseDouble(parts[6])));
            plan.planned_risk = ParseDouble(parts[7]);
            plan.planned_reward = ParseDouble(parts[8]);
            plan.status = Trim(parts[9]);
            if (parts.size() >= 11)
                plan.thesis = Trim(parts[10]);
            if (plan.created_at.empty())
                plan.created_at = NowTimeLabel();
            if (plan.status.empty())
                plan.status = "Open";
            if (!plan.symbol.empty() && plan.entry > 0.0 && plan.shares > 0)
                plans.push_back(plan);
        }

        std::sort(plans.begin(), plans.end(), [](const TradePlan& a, const TradePlan& b) {
            if (a.status != b.status)
                return a.status < b.status;
            if (a.created_at != b.created_at)
                return a.created_at > b.created_at;
            return a.symbol < b.symbol;
        });
        return plans;
    }

    bool SaveTradePlans(const std::vector<TradePlan>& plans)
    {
        return SaveTradePlans(plans, TradePlansFile());
    }

    bool SaveTradePlans(const std::vector<TradePlan>& plans, const std::filesystem::path& path)
    {
        std::error_code ec;
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::trunc);
        if (!file)
            return false;

        for (const TradePlan& plan : plans)
        {
            if (Trim(plan.symbol).empty() || plan.entry <= 0.0 || plan.shares <= 0)
                continue;
            file << TsvField(plan.created_at.empty() ? NowTimeLabel() : plan.created_at) << '\t'
                 << TsvField(Upper(plan.symbol)) << '\t'
                 << TsvField(plan.rating) << '\t'
                 << plan.entry << '\t'
                 << plan.stop << '\t'
                 << plan.target << '\t'
                 << plan.shares << '\t'
                 << plan.planned_risk << '\t'
                 << plan.planned_reward << '\t'
                 << TsvField(plan.status.empty() ? "Open" : plan.status) << '\t'
                 << TsvField(plan.thesis) << '\n';
        }
        return true;
    }

    std::vector<const StockQuote*> FilterQuotes(const StockState& state, const std::string& filter, const std::string& search)
    {
        std::vector<const StockQuote*> quotes;
        const std::string f = Lower(filter.empty() ? "all" : filter);
        for (const StockQuote& quote : state.quotes)
        {
            bool keep = true;
            if (f == "gainers")
                keep = quote.change_percent >= 0.0;
            else if (f == "losers")
                keep = quote.change_percent < 0.0;
            else if (f == "etfs")
                keep = quote.sector.find("ETF") != std::string::npos;
            else if (f != "all")
                keep = Lower(quote.sector).find(f) != std::string::npos;

            if (keep && ContainsTerms(SearchHaystack(quote), search))
                quotes.push_back(&quote);
        }
        return quotes;
    }

    std::vector<int> FilterSignalIndexes(const StockState& state, const std::string& filter, const std::string& search)
    {
        std::vector<int> indexes;
        const std::string f = Lower(filter.empty() ? "all" : filter);
        for (int i = 0; i < static_cast<int>(state.signals.size()); ++i)
        {
            const StockSignal& signal = state.signals[static_cast<size_t>(i)];
            bool keep = true;
            if (f == "eligible")
                keep = signal.posture == "Eligible watch";
            else if (f == "bullish")
                keep = signal.score >= 64;
            else if (f == "risk")
                keep = signal.risk == "High";
            else if (f != "all")
                keep = Lower(signal.rating).find(f) != std::string::npos;

            if (keep && ContainsTerms(SignalHaystack(signal), search))
                indexes.push_back(i);
        }
        return indexes;
    }

    const StockQuote* FindQuote(const StockState& state, const std::string& symbol)
    {
        const std::string target = Upper(symbol);
        for (const StockQuote& quote : state.quotes)
        {
            if (Upper(quote.symbol) == target)
                return &quote;
        }
        return state.quotes.empty() ? nullptr : &state.quotes.front();
    }

    const StockSignal* FindSignal(const StockState& state, const std::string& symbol)
    {
        const std::string target = Upper(symbol);
        for (const StockSignal& signal : state.signals)
        {
            if (Upper(signal.symbol) == target)
                return &signal;
        }
        return state.signals.empty() ? nullptr : &state.signals.front();
    }

    int DataConfidencePenalty(const StockQuote& quote)
    {
        int penalty = 0;
        const std::string quality_text = quote.source + " " + quote.status + " " + quote.note + " " + quote.data_quality;
        if (!quote.live)
            penalty += 8;
        if (quote.market_cap_estimated || quote.beta_estimated || quote.fundamentals_estimated)
            penalty += 6;
        if (ContainsWord(quality_text, "demo") || ContainsWord(quality_text, "sample"))
            penalty += 5;
        if (ContainsWord(quality_text, "fallback") || ContainsWord(quality_text, "stale") || ContainsWord(quality_text, "cache"))
            penalty += 6;
        return ClampInt(penalty, 0, 25);
    }

    std::string DataQualityBadge(const StockQuote& quote)
    {
        const std::string quality_text = quote.source + " " + quote.status + " " + quote.note + " " + quote.data_quality;
        if (ContainsWord(quality_text, "stale") || ContainsWord(quality_text, "cache") || ContainsWord(quality_text, "fallback"))
            return "Cache/stale fallback";
        if (ContainsWord(quality_text, "demo") || ContainsWord(quality_text, "sample"))
            return "Demo / estimated";
        if (quote.market_cap_estimated || quote.beta_estimated || quote.fundamentals_estimated)
            return quote.live ? "Live quote / estimated metrics" : "Estimated metrics";
        return quote.live ? "Live provider-backed" : "Unverified";
    }

    std::vector<InfoItem> BuildQuoteDataQualityRows(const StockQuote& quote)
    {
        const int penalty = DataConfidencePenalty(quote);
        const std::string badge = DataQualityBadge(quote);
        return {
            { "Quote feed", quote.live ? "Live" : "Fallback", quote.source.empty() ? "Unknown" : quote.source, "", quote.timestamp.empty() ? "No fetch timestamp is available." : "Fetched or generated at " + quote.timestamp + ".", "quality", quote.live ? "Ready" : "Review", quote.source, quote.timestamp },
            { "Price/volume", quote.live ? "Provider-backed" : "Demo/cache", quote.price > 0.0 ? FormatCurrency(quote.price) : "Unavailable", "", quote.live ? "Price, range, previous close, and volume came from the quote feed." : "Price data is not from a live provider in this board.", "quality", quote.live ? "Ready" : "Review", quote.source, quote.timestamp },
            { "Fundamentals", quote.fundamentals_estimated ? "Estimated" : "Provider-backed", quote.fundamentals_estimated ? "Estimate" : "Reported", "", quote.fundamentals_estimated ? "Market cap, valuation, dividend yield, or company fundamentals need a fundamentals provider before they should drive decisions." : "Fundamental metrics are marked as provider-backed.", "quality", quote.fundamentals_estimated ? "Review" : "Ready", quote.source, quote.timestamp },
            { "Risk beta", quote.beta_estimated ? "Estimated" : "Provider-backed", quote.beta_estimated ? "Estimate" : "Reported", "", quote.beta_estimated ? "Beta and beta-driven portfolio risk are confidence-adjusted until historical/provider beta is available." : "Beta is not flagged as estimated.", "quality", quote.beta_estimated ? "Review" : "Ready", quote.source, quote.timestamp },
            { "Confidence decay", penalty > 0 ? "Applied" : "None", penalty > 0 ? "-" + std::to_string(penalty) : "0", "", badge + ". " + (quote.note.empty() ? "No provider limitation note was supplied." : quote.note), "quality", penalty > 0 ? "Review" : "Ready", quote.source, quote.timestamp }
        };
    }

    std::string FormatCurrency(double value)
    {
        char buffer[64]{};
        const char* sign = value < 0.0 ? "-" : "";
        std::snprintf(buffer, sizeof(buffer), "%s$%.2f", sign, std::fabs(value));
        return buffer;
    }

    std::string FormatCompactCurrency(double value)
    {
        const double abs_value = std::fabs(value);
        char buffer[64]{};
        if (abs_value >= 1000000000000.0)
            std::snprintf(buffer, sizeof(buffer), "$%.2fT", value / 1000000000000.0);
        else if (abs_value >= 1000000000.0)
            std::snprintf(buffer, sizeof(buffer), "$%.2fB", value / 1000000000.0);
        else if (abs_value >= 1000000.0)
            std::snprintf(buffer, sizeof(buffer), "$%.2fM", value / 1000000.0);
        else
            std::snprintf(buffer, sizeof(buffer), "$%.2f", value);
        return buffer;
    }

    std::string FormatPercent(double value)
    {
        char buffer[64]{};
        std::snprintf(buffer, sizeof(buffer), "%+.2f%%", value);
        return buffer;
    }

    std::string FormatVolume(long long value)
    {
        char buffer[64]{};
        const double v = static_cast<double>(value);
        if (std::llabs(value) >= 1000000000LL)
            std::snprintf(buffer, sizeof(buffer), "%.2fB", v / 1000000000.0);
        else if (std::llabs(value) >= 1000000LL)
            std::snprintf(buffer, sizeof(buffer), "%.2fM", v / 1000000.0);
        else if (std::llabs(value) >= 1000LL)
            std::snprintf(buffer, sizeof(buffer), "%.2fK", v / 1000.0);
        else
            std::snprintf(buffer, sizeof(buffer), "%lld", value);
        return buffer;
    }
}
