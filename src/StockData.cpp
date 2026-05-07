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
            quote.provider = quote.source;
            quote.fetched_at = quote.timestamp;
            quote.freshness = "Fallback";
            quote.delayed = true;
            quote.fallback = true;
            quote.data_confidence = 55;
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
                quote.timestamp = NowTimeLabel();
                quote.source = "Alpha Vantage";
                quote.status = "Provider notice";
                quote.data_quality = "Provider error / no usable quote";
                quote.provider = quote.source;
                quote.fetched_at = quote.timestamp;
                quote.freshness = "Error";
                quote.delayed = true;
                quote.fallback = true;
                quote.data_confidence = 20;
                quote.provider_error = quote.note;
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
            quote.provider = quote.source;
            quote.fetched_at = quote.timestamp;
            quote.freshness = "Delayed";
            quote.delayed = true;
            quote.fallback = false;
            quote.data_confidence = 78;
            quote.history = BuildHistory(quote.symbol, quote.price > 0.0 ? quote.price : 100.0, quote.change_percent);
            return quote;
        }

        struct MomentumComponents
        {
            int trend = 50;
            int volume = 50;
            int oscillator = 50;
            int breakout = 50;
            int sector = 50;
            int regime = 50;
            int score = 50;
            double sma20 = 0.0;
            double sma50 = 0.0;
            double roc5 = 0.0;
            double roc20 = 0.0;
            double roc60 = 0.0;
            double rsi14 = 50.0;
            double macd_histogram = 0.0;
            double recent_high = 0.0;
            double recent_low = 0.0;
            double atr = 0.0;
            bool breakout_confirmed = false;
        };

        std::vector<double> PriceSeries(const StockQuote& quote)
        {
            std::vector<double> values;
            values.reserve(quote.history.size() + 3);
            if (quote.previous_close > 0.0)
                values.push_back(quote.previous_close);
            for (const float value : quote.history)
            {
                if (std::isfinite(value) && value > 0.0f)
                    values.push_back(static_cast<double>(value));
            }
            if (quote.price > 0.0)
            {
                if (values.empty() || std::fabs(values.back() - quote.price) > 0.0001)
                    values.push_back(quote.price);
            }
            if (values.empty())
                values.push_back(1.0);
            return values;
        }

        double AverageWindow(const std::vector<double>& values, size_t count, size_t offset_from_end = 0)
        {
            if (values.empty())
                return 0.0;
            const size_t end = values.size() > offset_from_end ? values.size() - offset_from_end : 0;
            if (end == 0)
                return values.front();
            const size_t start = count >= end ? 0 : end - count;
            double sum = 0.0;
            for (size_t i = start; i < end; ++i)
                sum += values[i];
            return sum / static_cast<double>(end - start);
        }

        double HighWindowBeforeLast(const std::vector<double>& values, size_t count)
        {
            if (values.size() <= 1)
                return values.empty() ? 0.0 : values.front();
            const size_t end = values.size() - 1;
            const size_t start = count >= end ? 0 : end - count;
            double high = values[start];
            for (size_t i = start + 1; i < end; ++i)
                high = std::max(high, values[i]);
            return high;
        }

        double LowWindow(const std::vector<double>& values, size_t count)
        {
            if (values.empty())
                return 0.0;
            const size_t start = count >= values.size() ? 0 : values.size() - count;
            double low = values[start];
            for (size_t i = start + 1; i < values.size(); ++i)
                low = std::min(low, values[i]);
            return low;
        }

        double RateOfChange(const std::vector<double>& values, size_t lookback)
        {
            if (values.size() <= 1)
                return 0.0;
            const size_t last = values.size() - 1;
            const size_t index = lookback >= values.size() ? 0 : last - lookback;
            const double base = values[index];
            return base > 0.0 ? ((values.back() - base) / base) * 100.0 : 0.0;
        }

        double Rsi14(const std::vector<double>& values)
        {
            if (values.size() < 2)
                return 50.0;
            const size_t periods = std::min<size_t>(14, values.size() - 1);
            double gains = 0.0;
            double losses = 0.0;
            for (size_t i = values.size() - periods; i < values.size(); ++i)
            {
                const double delta = values[i] - values[i - 1];
                if (delta >= 0.0)
                    gains += delta;
                else
                    losses -= delta;
            }
            const double average_gain = gains / static_cast<double>(periods);
            const double average_loss = losses / static_cast<double>(periods);
            if (average_loss <= 0.000001)
                return average_gain > 0.0 ? 99.0 : 50.0;
            const double rs = average_gain / average_loss;
            return 100.0 - (100.0 / (1.0 + rs));
        }

        double MacdHistogram(const std::vector<double>& values)
        {
            if (values.empty())
                return 0.0;
            const double alpha12 = 2.0 / 13.0;
            const double alpha26 = 2.0 / 27.0;
            const double alpha9 = 2.0 / 10.0;
            double ema12 = values.front();
            double ema26 = values.front();
            double signal = 0.0;
            bool seeded = false;
            double macd = 0.0;
            for (const double value : values)
            {
                ema12 = ema12 + alpha12 * (value - ema12);
                ema26 = ema26 + alpha26 * (value - ema26);
                macd = ema12 - ema26;
                if (!seeded)
                {
                    signal = macd;
                    seeded = true;
                }
                else
                {
                    signal = signal + alpha9 * (macd - signal);
                }
            }
            return macd - signal;
        }

        double AverageTrueRangeProxy(const StockQuote& quote, const std::vector<double>& values)
        {
            if (values.empty())
                return 0.0;

            const size_t periods = std::min<size_t>(14, values.size() > 1 ? values.size() - 1 : 0);
            double total = 0.0;
            int samples = 0;
            for (size_t i = values.size() > periods ? values.size() - periods : 1; i < values.size(); ++i)
            {
                if (i == 0)
                    continue;
                total += std::fabs(values[i] - values[i - 1]);
                ++samples;
            }

            const double close_to_close = samples > 0 ? total / static_cast<double>(samples) : values.back() * 0.02;
            const double current_range = quote.high > quote.low && quote.low > 0.0 ? quote.high - quote.low : 0.0;
            const double proxy = current_range > 0.0 ? std::max(close_to_close, current_range * 0.55) : close_to_close;
            return std::max(values.back() * 0.003, proxy);
        }

        std::string FormatRatio(double value)
        {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%.1fR", value);
            return buffer;
        }

        int BuildRiskScore(const StockQuote& quote, double entry_price, double stop_level)
        {
            const double range = quote.price > 0.0 ? ((quote.high - quote.low) / quote.price) * 100.0 : 0.0;
            const double stop_distance = entry_price > 0.0 ? ((entry_price - stop_level) / entry_price) * 100.0 : 0.0;
            int risk = 24;
            risk += static_cast<int>(std::round(range * 5.5));
            risk += static_cast<int>(std::round(std::max(0.0, quote.beta - 1.0) * 20.0));
            risk += static_cast<int>(std::round(stop_distance * 2.0));
            risk += DataConfidencePenalty(quote) / 2;
            if (quote.fallback)
                risk += 8;
            return ClampInt(risk, 5, 95);
        }

        std::string RiskLabelForScore(int risk_score)
        {
            if (risk_score >= 72)
                return "High";
            if (risk_score >= 50)
                return "Medium";
            return "Lower";
        }

        MomentumComponents BuildMomentumComponents(const StockQuote& quote)
        {
            MomentumComponents components;
            const std::vector<double> prices = PriceSeries(quote);
            const double price = quote.price > 0.0 ? quote.price : prices.back();
            components.sma20 = AverageWindow(prices, 20);
            components.sma50 = AverageWindow(prices, 50);
            const double prior_sma50 = AverageWindow(prices, 50, std::min<size_t>(5, prices.size() > 1 ? prices.size() - 1 : 0));
            components.roc5 = RateOfChange(prices, 5);
            components.roc20 = RateOfChange(prices, 20);
            components.roc60 = RateOfChange(prices, 60);
            components.rsi14 = Rsi14(prices);
            components.macd_histogram = MacdHistogram(prices);
            components.recent_high = HighWindowBeforeLast(prices, 30);
            components.recent_low = LowWindow(prices, 12);
            components.atr = AverageTrueRangeProxy(quote, prices);
            components.breakout_confirmed = components.recent_high > 0.0 && price > components.recent_high * 1.001 && quote.volume > 1000000;

            int trend = 42;
            if (price > components.sma20 && components.sma20 > 0.0)
                trend += 16;
            else
                trend -= 10;
            if (price > components.sma50 && components.sma50 > 0.0)
                trend += 15;
            else
                trend -= 8;
            if (components.sma20 > components.sma50 && components.sma50 > 0.0)
                trend += 12;
            if (components.sma50 >= prior_sma50 && prior_sma50 > 0.0)
                trend += 8;
            if (components.roc5 > 0.0)
                trend += 5;
            if (components.roc20 > 0.0)
                trend += 5;
            components.trend = ClampInt(trend, 0, 100);

            int volume = 38;
            if (quote.volume > 50000000)
                volume += 34;
            else if (quote.volume > 25000000)
                volume += 27;
            else if (quote.volume > 10000000)
                volume += 20;
            else if (quote.volume > 2500000)
                volume += 12;
            else if (quote.volume > 750000)
                volume += 6;
            if (quote.change_percent > 0.0)
                volume += 9;
            if (quote.price > quote.open && quote.open > 0.0)
                volume += 6;
            if (quote.change_percent < 0.0)
                volume -= 8;
            components.volume = ClampInt(volume, 0, 100);

            int oscillator = 48;
            if (components.rsi14 >= 50.0 && components.rsi14 <= 70.0)
                oscillator += 22;
            else if (components.rsi14 > 70.0 && components.rsi14 <= 75.0)
                oscillator += 8;
            else if (components.rsi14 > 75.0)
                oscillator -= 14;
            else if (components.rsi14 < 40.0)
                oscillator -= 18;
            if (components.macd_histogram > 0.0)
                oscillator += 16;
            else
                oscillator -= 10;
            if (components.roc5 > 0.0 && components.roc20 > 0.0)
                oscillator += 6;
            components.oscillator = ClampInt(oscillator, 0, 100);

            int breakout = 42;
            if (components.breakout_confirmed)
                breakout += 34;
            else if (components.recent_high > 0.0 && price >= components.recent_high * 0.985)
                breakout += 14;
            if (components.volume >= 70)
                breakout += 8;
            if (quote.change_percent > 1.0)
                breakout += 6;
            if (price < components.sma20 && components.sma20 > 0.0)
                breakout -= 10;
            components.breakout = ClampInt(breakout, 0, 100);

            int sector = 52;
            if (!quote.sector.empty() && quote.sector != "Watchlist")
                sector += 4;
            if ((ContainsWord(quote.sector, "Technology") || ContainsWord(quote.sector, "Semiconductor") || ContainsWord(quote.sector, "Growth")) && quote.change_percent > 0.0)
                sector += 9;
            if (quote.change_percent < -1.0)
                sector -= 10;
            if (quote.beta > 1.7)
                sector -= 6;
            components.sector = ClampInt(sector, 0, 100);

            const double intraday_range = price > 0.0 ? ((quote.high - quote.low) / price) * 100.0 : 0.0;
            int regime = 55;
            if (quote.change_percent > 0.75)
                regime += 10;
            if (quote.change_percent < -0.75)
                regime -= 12;
            if (intraday_range > 5.0)
                regime -= 10;
            if (quote.beta > 1.6)
                regime -= 7;
            if (quote.symbol == "SPY" || quote.symbol == "QQQ")
                regime += 5;
            components.regime = ClampInt(regime, 0, 100);

            const int weighted =
                (components.trend * 25) +
                (components.volume * 20) +
                (components.oscillator * 20) +
                (components.breakout * 15) +
                (components.sector * 10) +
                (components.regime * 10);
            components.score = ClampInt(static_cast<int>(std::round(static_cast<double>(weighted) / 100.0)), 0, 100);
            return components;
        }

        int BuildScore(const StockQuote& quote)
        {
            return BuildMomentumComponents(quote).score;
        }

        std::string RatingForScore(int score)
        {
            if (score >= 90)
                return "High-conviction paper setup";
            if (score >= 75)
                return "Strong paper setup";
            if (score >= 60)
                return "Watchlist setup";
            if (score >= 40)
                return "Neutral research";
            return "Weak momentum";
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
            const MomentumComponents components = BuildMomentumComponents(quote);
            StockSignal signal;
            signal.symbol = quote.symbol;
            signal.company = quote.name;
            signal.score = components.score;
            const int source_bonus = quote.source == "Alpha Vantage" && quote.live ? 5 : -3;
            const int confidence_penalty = DataConfidencePenalty(quote);
            signal.confidence = ClampInt(signal.score + source_bonus - confidence_penalty, 10, 96);
            signal.rating = RatingForScore(signal.score);
            signal.setup_type = signal.score >= 75 ? "Paper Long Setup Detected" :
                signal.score >= 60 ? "Paper Watch Setup" :
                signal.score >= 40 ? "No Paper Setup" :
                "Momentum Failure Review";
            signal.posture = signal.score >= config.min_conviction ? "Manual review queue" : "Research watch";
            signal.horizon = signal.score >= 70 ? "2-8 weeks" : "1-4 weeks";
            const double price = quote.price > 0.0 ? quote.price : 1.0;
            const double range = price > 0.0 ? ((quote.high - quote.low) / price) * 100.0 : 0.0;
            const double stop_percent = ClampDouble(0.045 + (quote.beta * 0.012) + (range / 220.0), 0.045, 0.14);
            signal.entry_price = signal.score >= 75 ?
                std::max(price * 1.002, components.breakout_confirmed ? components.recent_high * 1.001 : price * 1.002) :
                price;
            signal.stop_level = price * (1.0 - stop_percent);
            if (components.recent_low > 0.0 && components.recent_low < price && components.recent_low > signal.stop_level)
                signal.stop_level = components.recent_low * 0.995;
            if (signal.stop_level >= signal.entry_price)
                signal.stop_level = signal.entry_price * (1.0 - stop_percent);
            const double planned_risk = std::max(signal.entry_price * 0.0001, signal.entry_price - signal.stop_level);
            signal.atr_value = std::max(components.atr, signal.entry_price * 0.003);
            signal.resistance_level = components.recent_high > signal.entry_price ? components.recent_high : 0.0;
            const double target_2r = signal.entry_price + planned_risk * 2.0;
            const double atr_moderate = signal.entry_price + signal.atr_value * 2.0;
            signal.target1_price = std::max(target_2r, atr_moderate);
            if (signal.resistance_level > signal.entry_price && signal.resistance_level >= signal.target1_price * 0.985)
                signal.target2_price = std::max(signal.target1_price, signal.resistance_level);
            else
            {
                const double continuation_step = std::max(std::min(signal.atr_value, planned_risk * 0.75), signal.entry_price * 0.003);
                const double continuation_target = signal.entry_price + std::max(planned_risk * 3.0, signal.atr_value * 3.0);
                signal.target2_price = std::max(signal.target1_price + continuation_step, continuation_target);
            }
            const double max_reasonable_target = signal.entry_price + std::max(planned_risk * 3.5, signal.atr_value * 4.0);
            signal.target2_price = std::min(signal.target2_price, max_reasonable_target);
            signal.target_price = signal.target2_price;
            signal.trailing_stop_level = std::max(signal.stop_level, signal.entry_price - signal.atr_value * 2.0);
            signal.risk_reward = planned_risk > 0.0 ? (signal.target1_price - signal.entry_price) / planned_risk : 0.0;
            signal.risk_score = BuildRiskScore(quote, signal.entry_price, signal.stop_level);
            signal.manual_confirmation_required = true;
            signal.paper_only = true;
            signal.entry_idea = "Paper entry only above " + FormatCurrency(signal.entry_price) + " after manual confirmation.";
            signal.invalidation = "Invalidated if price closes below " + FormatCurrency(signal.stop_level) + " or the momentum score falls below 50.";
            signal.expected_sell_zone = FormatCurrency(signal.target1_price) + " to " + FormatCurrency(signal.target2_price);
            signal.exit_plan = "Scale paper exits: review 50% near " + FormatCurrency(signal.target1_price) +
                ", review 25% near " + FormatCurrency(signal.target2_price) +
                ", and trail the remaining 25% at 2x ATR below the highest close.";
            signal.position_budget = std::min(config.max_position_amount, config.portfolio_cash * (config.max_portfolio_risk_percent / 100.0) * 8.0);
            signal.upside = FormatPercent(signal.entry_price > 0.0 ? ((signal.target_price - signal.entry_price) / signal.entry_price) * 100.0 : 0.0);
            signal.downside = FormatPercent(signal.entry_price > 0.0 ? ((signal.stop_level - signal.entry_price) / signal.entry_price) * 100.0 : 0.0);
            signal.risk = RiskLabelForScore(signal.risk_score);
            signal.source = quote.source;

            if (signal.score >= 90)
                signal.thesis = quote.symbol + " has aligned trend, participation, and regime inputs for a high-conviction paper setup. The exit zone is probabilistic and still requires confirmation, not a guaranteed price.";
            else if (signal.score >= 75)
                signal.thesis = quote.symbol + " shows enough aligned momentum to draft a paper long plan with explicit invalidation, partial exit zones, and sizing limits.";
            else if (signal.score >= 60)
                signal.thesis = quote.symbol + " is improving, but the setup still needs confirmation from follow-through, volume, and broader market tone.";
            else if (signal.score >= 40)
                signal.thesis = quote.symbol + " is balanced. Auralith keeps it visible for research without creating a paper execution plan.";
            else
                signal.thesis = quote.symbol + " is showing weak or deteriorating momentum, so the review should focus on stops, exposure, and risk reduction.";

            signal.factors = {
                {"Trend", "", std::to_string(components.trend) + "/100", "25%", "Price versus 20-period and 50-period averages, plus the slope of the longer average.", "Trend"},
                {"Rate of change", "", "5d " + FormatPercent(components.roc5) + " / 20d " + FormatPercent(components.roc20), "", "Positive short and swing rate-of-change improves the momentum setup.", "ROC"},
                {"RSI / MACD", "", "RSI " + std::to_string(static_cast<int>(std::round(components.rsi14))) + " / MACD " + (components.macd_histogram >= 0.0 ? "positive" : "negative"), "20%", "Healthy momentum prefers RSI 50-70 and a rising MACD profile without overextension.", "Oscillator"},
                {"Volume confirmation", "", FormatVolume(quote.volume), "20%", "Higher participation improves confidence that price movement has observable support.", "Volume"},
                {"Breakout strength", "", components.breakout_confirmed ? "Confirmed" : "Watching", "15%", "A breakout requires price above recent resistance with volume confirmation.", "Breakout"},
                {"Sector and regime", "", std::to_string((components.sector + components.regime) / 2) + "/100", "20%", "Sector proxy and market-regime proxy reduce setup quality when broader risk is elevated.", "Context"},
                {"Paper setup boundary", "", signal.entry_idea, "", signal.invalidation, "Paper"},
                {"Target 1", "", FormatCurrency(signal.target1_price), "50%", "First expected sell zone uses at least 2R discipline and ATR realism.", "Exit"},
                {"Target 2", "", FormatCurrency(signal.target2_price), "25%", "Second expected sell zone considers nearby resistance and a capped ATR continuation path.", "Exit"},
                {"Trailing stop", "", FormatCurrency(signal.trailing_stop_level), "25%", "Trail the remaining paper position at 2x ATR below the highest close while momentum holds.", "Exit"},
                {"ATR / resistance", "", FormatCurrency(signal.atr_value) + " / " + (signal.resistance_level > 0.0 ? FormatCurrency(signal.resistance_level) : "None above entry"), "", "ATR keeps expected sell zones realistic; resistance marks prior supply where momentum may stall.", "Path"},
                {"Risk / reward", "", FormatRatio(signal.risk_reward), std::to_string(signal.risk_score) + "/100 risk", "Targets are expected sell zones for paper planning, not guaranteed sell prices.", "Risk"},
                {"Data confidence", "", std::to_string(signal.confidence) + "%", "-" + std::to_string(confidence_penalty), DataQualityBadge(quote) + ". " + quote.note, "Freshness"},
                {"Risk budget", "", FormatCurrency(signal.position_budget), "", "Position sizing is capped by the configured paper portfolio controls.", "Sizing"},
                {"Manual confirmation", "", "Required", "", "Auralith can draft the paper plan, but a user must confirm before any simulated execution is recorded.", "Safety"}
            };
            return signal;
        }

        std::string SearchHaystack(const StockQuote& quote)
        {
            return Lower(quote.symbol + " " + quote.name + " " + quote.sector + " " + quote.status + " " + quote.source);
        }

        std::string SignalHaystack(const StockSignal& signal)
        {
            return Lower(signal.symbol + " " + signal.company + " " + signal.rating + " " + signal.setup_type + " " + signal.posture + " " + signal.risk + " " + signal.thesis);
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
            demo.source_label = "No Alpha Vantage key is saved, so Auralith is showing the built-in sample board.";
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
                ? "Direct quote refresh returned no usable symbols, so Auralith is showing demo data."
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
                keep = signal.posture == "Manual review queue";
            else if (f == "paper")
                keep = signal.setup_type.find("Paper") != std::string::npos && signal.score >= 60;
            else if (f == "risk")
                keep = signal.risk == "High";
            else if (f == "neutral")
                keep = signal.rating == "Neutral research";
            else if (f == "weak")
                keep = signal.rating == "Weak momentum";
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
        const std::string quality_text = quote.source + " " + quote.status + " " + quote.note + " " + quote.data_quality + " " + quote.freshness + " " + quote.provider_error;
        if (!quote.live)
            penalty += 8;
        if (quote.fallback)
            penalty += 6;
        if (quote.delayed)
            penalty += 3;
        if (quote.market_cap_estimated || quote.beta_estimated || quote.fundamentals_estimated)
            penalty += 6;
        if (ContainsWord(quality_text, "demo") || ContainsWord(quality_text, "sample"))
            penalty += 5;
        if (ContainsWord(quality_text, "fallback") || ContainsWord(quality_text, "stale") || ContainsWord(quality_text, "cache"))
            penalty += 6;
        if (ContainsWord(quality_text, "error") || ContainsWord(quality_text, "failed"))
            penalty += 8;
        return ClampInt(penalty, 0, 25);
    }

    std::string DataQualityBadge(const StockQuote& quote)
    {
        const std::string quality_text = quote.source + " " + quote.status + " " + quote.note + " " + quote.data_quality + " " + quote.freshness + " " + quote.provider_error;
        if (ContainsWord(quality_text, "error") || ContainsWord(quality_text, "failed"))
            return "Provider error";
        if (ContainsWord(quality_text, "stale") || ContainsWord(quality_text, "cache") || ContainsWord(quality_text, "fallback"))
            return "Cache/stale fallback";
        if (quote.delayed || ContainsWord(quality_text, "delayed"))
            return quote.live ? "Delayed provider-backed" : "Delayed fallback";
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
        const std::string provider = quote.provider.empty() ? (quote.source.empty() ? "Unknown" : quote.source) : quote.provider;
        const std::string fetched_at = quote.fetched_at.empty() ? quote.timestamp : quote.fetched_at;
        const int confidence = quote.data_confidence > 0 ? quote.data_confidence : ClampInt(100 - penalty, 25, 100);
        return {
            { "Quote feed", quote.live ? "Provider-backed" : "Fallback", provider, "", fetched_at.empty() ? "No fetch timestamp is available." : "Fetched or generated at " + fetched_at + ".", "quality", quote.live ? "Ready" : "Review", provider, fetched_at },
            { "Freshness", quote.freshness.empty() ? badge : quote.freshness, quote.fallback ? "Fallback" : (quote.delayed ? "Delayed" : "Live"), "", "Auralith treats freshness as part of the data contract, not UI decoration.", "quality", quote.fallback || quote.delayed ? "Review" : "Ready", provider, fetched_at },
            { "Price/volume", quote.live ? "Provider-backed" : "Demo/cache", quote.price > 0.0 ? FormatCurrency(quote.price) : "Unavailable", "", quote.live ? "Price, range, previous close, and volume came from the quote feed." : "Price data is not from a live provider in this board.", "quality", quote.live ? "Ready" : "Review", provider, fetched_at },
            { "Fundamentals", quote.fundamentals_estimated ? "Estimated" : "Provider-backed", quote.fundamentals_estimated ? "Estimate" : "Reported", "", quote.fundamentals_estimated ? "Market cap, valuation, dividend yield, or company fundamentals need a fundamentals provider before they should drive decisions." : "Fundamental metrics are marked as provider-backed.", "quality", quote.fundamentals_estimated ? "Review" : "Ready", provider, fetched_at },
            { "Risk beta", quote.beta_estimated ? "Estimated" : "Provider-backed", quote.beta_estimated ? "Estimate" : "Reported", "", quote.beta_estimated ? "Beta and beta-driven portfolio risk are confidence-adjusted until historical/provider beta is available." : "Beta is not flagged as estimated.", "quality", quote.beta_estimated ? "Review" : "Ready", provider, fetched_at },
            { "Research confidence", confidence >= 75 ? "High" : (confidence >= 50 ? "Medium" : "Low"), std::to_string(confidence) + "%", "", badge + ". Confidence is reduced by fallback, stale/delayed feeds, provider errors, and estimated fundamentals.", "quality", confidence >= 75 ? "Ready" : "Review", provider, fetched_at },
            { "Confidence decay", penalty > 0 ? "Applied" : "None", penalty > 0 ? "-" + std::to_string(penalty) : "0", "", badge + ". " + (quote.note.empty() ? "No provider limitation note was supplied." : quote.note), "quality", penalty > 0 ? "Review" : "Ready", provider, fetched_at }
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
