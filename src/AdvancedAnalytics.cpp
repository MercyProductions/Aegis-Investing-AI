#include "AdvancedAnalytics.h"

#include "Json.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>

namespace aegis
{
    namespace
    {
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

        double Clamp(double value, double low, double high)
        {
            return std::max(low, std::min(high, value));
        }

        double Average(const std::vector<double>& values, size_t start, size_t count)
        {
            if (values.empty() || start >= values.size())
                return 0.0;
            const size_t end = std::min(values.size(), start + count);
            if (end <= start)
                return 0.0;
            double sum = 0.0;
            for (size_t i = start; i < end; ++i)
                sum += values[i];
            return sum / static_cast<double>(end - start);
        }

        double LastSma(const std::vector<double>& values, size_t period)
        {
            if (values.empty())
                return 0.0;
            if (values.size() < period)
                return Average(values, 0, values.size());
            return Average(values, values.size() - period, period);
        }

        double LastEma(const std::vector<double>& values, size_t period)
        {
            if (values.empty())
                return 0.0;
            const double k = 2.0 / (static_cast<double>(period) + 1.0);
            double ema = values.front();
            for (double value : values)
                ema = value * k + ema * (1.0 - k);
            return ema;
        }

        std::vector<double> EmaSeries(const std::vector<double>& values, size_t period)
        {
            std::vector<double> out;
            if (values.empty())
                return out;
            out.reserve(values.size());
            const double k = 2.0 / (static_cast<double>(period) + 1.0);
            double ema = values.front();
            for (double value : values)
            {
                ema = value * k + ema * (1.0 - k);
                out.push_back(ema);
            }
            return out;
        }

        std::filesystem::path NotesFile()
        {
            return AppDataDirectory() / "symbol-notes.tsv";
        }

        std::filesystem::path JournalFile()
        {
            return AppDataDirectory() / "trade-journal-upgraded.tsv";
        }

        std::filesystem::path HistoryCacheDirectory()
        {
            return AppDataDirectory() / "history-cache";
        }

        std::filesystem::path HistoryCacheFile(const std::string& symbol)
        {
            return HistoryCacheDirectory() / (Upper(symbol) + ".daily.tsv");
        }

        std::filesystem::path ResearchCacheDirectory()
        {
            return AppDataDirectory() / "research-cache";
        }

        std::filesystem::path ResearchCacheFile(const std::string& symbol)
        {
            return ResearchCacheDirectory() / (Upper(symbol) + ".bundle.tsv");
        }

        std::filesystem::path SecCacheDirectory()
        {
            return AppDataDirectory() / "sec-cache";
        }

        std::filesystem::path SecTickerCacheFile()
        {
            return SecCacheDirectory() / "company-tickers.tsv";
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

        long long ParseLongLong(const std::string& value, long long fallback = 0)
        {
            try
            {
                return std::stoll(Trim(value));
            }
            catch (...)
            {
                return fallback;
            }
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

        std::string DemoDate(int index_from_now)
        {
            return "T-" + std::to_string(index_from_now);
        }

        std::string ConfiguredAlphaKey(const Config& config)
        {
            const std::string saved = Trim(config.alpha_vantage_api_key);
            if (!saved.empty())
                return saved;
            return Trim(GetEnvUtf8(L"AEGIS_ALPHA_VANTAGE_API_KEY"));
        }

        bool IsCacheFresh(const std::filesystem::path& path, int max_age_hours)
        {
            std::error_code ec;
            const auto modified = std::filesystem::last_write_time(path, ec);
            if (ec)
                return false;
            const auto now = std::filesystem::file_time_type::clock::now();
            return now - modified < std::chrono::hours(std::max(1, max_age_hours));
        }

        std::vector<Candle> TrimCandles(std::vector<Candle> candles, int days)
        {
            const int limit = std::max(60, days);
            if (static_cast<int>(candles.size()) > limit)
                candles.erase(candles.begin(), candles.end() - limit);
            return candles;
        }

        std::vector<Candle> LoadCachedCandles(const std::string& symbol, int days)
        {
            std::vector<Candle> candles;
            std::ifstream file(HistoryCacheFile(symbol));
            if (!file)
                return candles;
            std::string line;
            while (std::getline(file, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                const std::vector<std::string> parts = SplitTabs(line);
                if (parts.size() < 6 || Lower(parts[0]) == "date")
                    continue;
                Candle candle;
                candle.date = Trim(parts[0]);
                candle.open = ParseDouble(parts[1]);
                candle.high = ParseDouble(parts[2]);
                candle.low = ParseDouble(parts[3]);
                candle.close = ParseDouble(parts[4]);
                candle.volume = ParseLongLong(parts[5]);
                if (!candle.date.empty() && candle.close > 0.0)
                    candles.push_back(candle);
            }
            std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                return a.date < b.date;
            });
            return TrimCandles(std::move(candles), days);
        }

        bool SaveCachedCandles(const std::string& symbol, const std::vector<Candle>& candles)
        {
            std::error_code ec;
            std::filesystem::create_directories(HistoryCacheDirectory(), ec);
            std::ofstream file(HistoryCacheFile(symbol), std::ios::trunc);
            if (!file)
                return false;
            file << "date\topen\thigh\tlow\tclose\tvolume\n";
            for (const Candle& candle : candles)
            {
                file << TsvField(candle.date) << '\t'
                     << candle.open << '\t'
                     << candle.high << '\t'
                     << candle.low << '\t'
                     << candle.close << '\t'
                     << candle.volume << '\n';
            }
            return true;
        }

        std::vector<Candle> ParseAlphaDailyCandles(const JsonValue& root, int days)
        {
            const JsonValue& series = root["Time Series (Daily)"];
            if (!series.IsObject())
                return {};

            std::vector<Candle> candles;
            candles.reserve(series.object_value.size());
            for (const auto& pair : series.object_value)
            {
                const JsonValue& row = pair.second;
                Candle candle;
                candle.date = pair.first;
                candle.open = row["1. open"].AsDouble(ParseDouble(row["1. open"].AsString()));
                candle.high = row["2. high"].AsDouble(ParseDouble(row["2. high"].AsString()));
                candle.low = row["3. low"].AsDouble(ParseDouble(row["3. low"].AsString()));
                candle.close = row["4. close"].AsDouble(ParseDouble(row["4. close"].AsString()));
                candle.volume = ParseLongLong(row["5. volume"].AsString());
                if (candle.close > 0.0)
                    candles.push_back(candle);
            }
            std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                return a.date < b.date;
            });
            return TrimCandles(std::move(candles), days);
        }

        double ReadJsonDouble(const JsonValue& object, const std::string& key, double fallback = 0.0)
        {
            const JsonValue& value = object[key];
            if (value.IsNumber())
                return value.number_value;
            return ParseDouble(value.AsString(), fallback);
        }

        double RatioToPercent(double value)
        {
            if (std::fabs(value) <= 2.0)
                return value * 100.0;
            return value;
        }

        FundamentalSnapshot ParseAlphaOverview(const StockQuote& quote, const JsonValue& root)
        {
            FundamentalSnapshot f = BuildFundamentals(quote);
            const std::string symbol = root["Symbol"].AsString(quote.symbol);
            f.symbol = Upper(symbol);
            f.revenue = ReadJsonDouble(root, "RevenueTTM", f.revenue);
            f.eps = ReadJsonDouble(root, "EPS", f.eps);
            const double gross_profit = ReadJsonDouble(root, "GrossProfitTTM", 0.0);
            if (gross_profit > 0.0 && f.revenue > 0.0)
                f.gross_margin = (gross_profit / f.revenue) * 100.0;
            f.net_margin = RatioToPercent(ReadJsonDouble(root, "ProfitMargin", f.net_margin));
            f.debt_to_equity = RatioToPercent(ReadJsonDouble(root, "DebtToEquity", f.debt_to_equity));
            f.free_cash_flow = ReadJsonDouble(root, "EBITDA", f.free_cash_flow);
            f.pe = ReadJsonDouble(root, "PERatio", f.pe);
            f.peg = ReadJsonDouble(root, "PEGRatio", f.peg);
            f.dividend_yield = RatioToPercent(ReadJsonDouble(root, "DividendYield", f.dividend_yield));
            const std::string description = root["Description"].AsString();
            f.summary = description.empty()
                ? f.symbol + " fundamentals loaded from Alpha Vantage OVERVIEW."
                : description;
            return f;
        }

        std::string NewsSentimentForSymbol(const JsonValue& article, const std::string& symbol)
        {
            const JsonValue& ticker_sentiment = article["ticker_sentiment"];
            if (ticker_sentiment.IsArray())
            {
                for (const JsonValue& item : ticker_sentiment.array_value)
                {
                    if (Upper(item["ticker"].AsString()) == Upper(symbol))
                    {
                        const std::string label = item["ticker_sentiment_label"].AsString();
                        if (!label.empty())
                            return label;
                    }
                }
            }
            return article["overall_sentiment_label"].AsString("Neutral");
        }

        std::vector<NewsItem> ParseAlphaNews(const std::string& symbol, const JsonValue& root)
        {
            std::vector<NewsItem> rows;
            const JsonValue& feed = root["feed"];
            if (!feed.IsArray())
                return rows;
            for (const JsonValue& article : feed.array_value)
            {
                NewsItem item;
                item.date = article["time_published"].AsString("Recent");
                if (item.date.size() >= 8)
                    item.date = item.date.substr(0, 4) + "-" + item.date.substr(4, 2) + "-" + item.date.substr(6, 2);
                item.source = article["source"].AsString("Alpha Vantage");
                item.title = article["title"].AsString();
                item.sentiment = NewsSentimentForSymbol(article, symbol);
                item.summary = article["summary"].AsString();
                item.url = article["url"].AsString();
                if (!item.title.empty())
                    rows.push_back(std::move(item));
                if (rows.size() >= 8)
                    break;
            }
            return rows;
        }

        std::vector<EarningsItem> ParseAlphaEarnings(const JsonValue& root)
        {
            std::vector<EarningsItem> rows;
            const JsonValue& quarterly = root["quarterlyEarnings"];
            if (!quarterly.IsArray())
                return rows;
            for (const JsonValue& item : quarterly.array_value)
            {
                EarningsItem row;
                row.date = item["reportedDate"].AsString(item["fiscalDateEnding"].AsString("Recent"));
                row.status = "Reported";
                row.eps_estimate = ReadJsonDouble(item, "estimatedEPS", 0.0);
                row.eps_actual = ReadJsonDouble(item, "reportedEPS", 0.0);
                row.surprise = ReadJsonDouble(item, "surprisePercentage", 0.0);
                row.estimated_move = std::fabs(row.surprise) * 0.35;
                row.drift = row.surprise * 0.20;
                rows.push_back(row);
                if (rows.size() >= 8)
                    break;
            }
            return rows;
        }

        bool SaveResearchCache(const std::string& symbol, const ResearchBundleResult& bundle)
        {
            std::error_code ec;
            std::filesystem::create_directories(ResearchCacheDirectory(), ec);
            std::ofstream file(ResearchCacheFile(symbol), std::ios::trunc);
            if (!file)
                return false;
            const FundamentalSnapshot& f = bundle.fundamentals;
            file << "FUNDAMENTAL" << '\t'
                 << TsvField(f.symbol) << '\t'
                 << f.revenue << '\t'
                 << f.eps << '\t'
                 << f.gross_margin << '\t'
                 << f.net_margin << '\t'
                 << f.debt_to_equity << '\t'
                 << f.free_cash_flow << '\t'
                 << f.pe << '\t'
                 << f.peg << '\t'
                 << f.dividend_yield << '\t'
                 << TsvField(f.summary) << '\n';
            for (const FilingItem& item : bundle.filings)
            {
                file << "FILING" << '\t'
                     << TsvField(item.form) << '\t'
                     << TsvField(item.date) << '\t'
                     << TsvField(item.title) << '\t'
                     << TsvField(item.url) << '\t'
                     << TsvField(item.summary) << '\t'
                     << TsvField(item.risk_change) << '\n';
            }
            for (const NewsItem& item : bundle.news)
            {
                file << "NEWS" << '\t'
                     << TsvField(item.date) << '\t'
                     << TsvField(item.source) << '\t'
                     << TsvField(item.title) << '\t'
                     << TsvField(item.sentiment) << '\t'
                     << TsvField(item.summary) << '\t'
                     << TsvField(item.url) << '\n';
            }
            for (const EarningsItem& item : bundle.earnings)
            {
                file << "EARNINGS" << '\t'
                     << TsvField(item.date) << '\t'
                     << TsvField(item.status) << '\t'
                     << item.eps_estimate << '\t'
                     << item.eps_actual << '\t'
                     << item.surprise << '\t'
                     << item.estimated_move << '\t'
                     << item.drift << '\n';
            }
            return true;
        }

        ResearchBundleResult LoadResearchCache(const StockQuote& quote)
        {
            ResearchBundleResult bundle;
            std::ifstream file(ResearchCacheFile(quote.symbol));
            if (!file)
                return bundle;
            std::string line;
            while (std::getline(file, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                const std::vector<std::string> parts = SplitTabs(line);
                if (parts.empty())
                    continue;
                if (parts[0] == "FUNDAMENTAL" && parts.size() >= 12)
                {
                    FundamentalSnapshot f;
                    f.symbol = Upper(Trim(parts[1]));
                    f.revenue = ParseDouble(parts[2]);
                    f.eps = ParseDouble(parts[3]);
                    f.gross_margin = ParseDouble(parts[4]);
                    f.net_margin = ParseDouble(parts[5]);
                    f.debt_to_equity = ParseDouble(parts[6]);
                    f.free_cash_flow = ParseDouble(parts[7]);
                    f.pe = ParseDouble(parts[8]);
                    f.peg = ParseDouble(parts[9]);
                    f.dividend_yield = ParseDouble(parts[10]);
                    f.summary = Trim(parts[11]);
                    bundle.fundamentals = std::move(f);
                }
                else if (parts[0] == "FILING" && parts.size() >= 7)
                {
                    bundle.filings.push_back({ Trim(parts[1]), Trim(parts[2]), Trim(parts[3]), Trim(parts[4]), Trim(parts[5]), Trim(parts[6]) });
                }
                else if (parts[0] == "NEWS" && parts.size() >= 7)
                {
                    bundle.news.push_back({ Trim(parts[1]), Trim(parts[2]), Trim(parts[3]), Trim(parts[4]), Trim(parts[5]), Trim(parts[6]) });
                }
                else if (parts[0] == "EARNINGS" && parts.size() >= 8)
                {
                    EarningsItem item;
                    item.date = Trim(parts[1]);
                    item.status = Trim(parts[2]);
                    item.eps_estimate = ParseDouble(parts[3]);
                    item.eps_actual = ParseDouble(parts[4]);
                    item.surprise = ParseDouble(parts[5]);
                    item.estimated_move = ParseDouble(parts[6]);
                    item.drift = ParseDouble(parts[7]);
                    bundle.earnings.push_back(item);
                }
            }
            bundle.ok = !bundle.fundamentals.symbol.empty() || !bundle.filings.empty() || !bundle.news.empty() || !bundle.earnings.empty();
            bundle.cached = bundle.ok;
            bundle.status = bundle.ok ? Upper(quote.symbol) + " research bundle loaded from cache." : "";
            return bundle;
        }

        std::string ProviderNote(const JsonValue& root)
        {
            return root["Note"].AsString(root["Information"].AsString(root["Error Message"].AsString()));
        }

        std::string TenDigitCik(int cik)
        {
            std::ostringstream stream;
            stream << std::setw(10) << std::setfill('0') << cik;
            return stream.str();
        }

        std::string UnpaddedCik(const std::string& cik)
        {
            std::string out = cik;
            out.erase(0, out.find_first_not_of('0'));
            return out.empty() ? "0" : out;
        }

        std::string FilingSummaryForForm(const std::string& form)
        {
            const std::string f = Upper(form);
            if (f.find("10-K") == 0)
                return "Annual report with audited statements, business overview, MD&A, and risk factors.";
            if (f.find("10-Q") == 0)
                return "Quarterly report covering interim financials, liquidity, and operational updates.";
            if (f.find("8-K") == 0)
                return "Current report for material company events, agreements, presentations, or leadership changes.";
            if (f == "4" || f == "3" || f == "5")
                return "Insider ownership transaction filing.";
            return "SEC filing surfaced from the company's recent submissions feed.";
        }

        std::string FilingRiskReadForForm(const std::string& form)
        {
            const std::string f = Upper(form);
            if (f.find("10-K") == 0)
                return "Compare risk-factor language against the prior annual report.";
            if (f.find("10-Q") == 0)
                return "Check for liquidity, margin, debt, guidance, or litigation changes.";
            if (f.find("8-K") == 0)
                return "Event-driven item; inspect trigger and market relevance.";
            if (f == "4" || f == "3" || f == "5")
                return "Review insider transaction size, direction, and clustering.";
            return "Manual review recommended.";
        }

        std::string ResolveSecCik(const std::string& symbol)
        {
            const std::string target = Upper(symbol);
            auto read_cache = [&]() -> std::string {
                std::ifstream file(SecTickerCacheFile());
                std::string line;
                while (std::getline(file, line))
                {
                    const std::vector<std::string> parts = SplitTabs(line);
                    if (parts.size() >= 2 && Upper(Trim(parts[0])) == target)
                        return Trim(parts[1]);
                }
                return {};
            };

            if (IsCacheFresh(SecTickerCacheFile(), 24))
            {
                const std::string cached = read_cache();
                if (!cached.empty())
                    return cached;
            }

            const HttpResponse response = HttpGet("https://www.sec.gov/files/company_tickers.json");
            if (response.error.empty() && response.status_code >= 200 && response.status_code < 300)
            {
                const JsonParseResult parsed = ParseJson(response.body);
                if (parsed.ok && parsed.value.IsObject())
                {
                    std::error_code ec;
                    std::filesystem::create_directories(SecCacheDirectory(), ec);
                    std::ofstream cache(SecTickerCacheFile(), std::ios::trunc);
                    std::string match;
                    for (const auto& pair : parsed.value.object_value)
                    {
                        const JsonValue& row = pair.second;
                        const std::string ticker = Upper(row["ticker"].AsString());
                        const int cik_int = row["cik_str"].AsInt(0);
                        if (ticker.empty() || cik_int <= 0)
                            continue;
                        const std::string cik = TenDigitCik(cik_int);
                        if (cache)
                            cache << TsvField(ticker) << '\t' << cik << '\t' << TsvField(row["title"].AsString()) << '\n';
                        if (ticker == target)
                            match = cik;
                    }
                    if (!match.empty())
                        return match;
                }
            }

            return read_cache();
        }

        std::vector<FilingItem> LoadSecFilings(const std::string& symbol)
        {
            const std::string cik = ResolveSecCik(symbol);
            if (cik.empty())
                return {};

            const std::string url = "https://data.sec.gov/submissions/CIK" + cik + ".json";
            const HttpResponse response = HttpGet(url);
            if (!response.error.empty() || response.status_code < 200 || response.status_code >= 300)
                return {};
            const JsonParseResult parsed = ParseJson(response.body);
            if (!parsed.ok)
                return {};

            const JsonValue& recent = parsed.value["filings"]["recent"];
            const JsonValue& forms = recent["form"];
            const JsonValue& dates = recent["filingDate"];
            const JsonValue& accession_numbers = recent["accessionNumber"];
            const JsonValue& primary_docs = recent["primaryDocument"];
            if (!forms.IsArray() || !dates.IsArray() || !accession_numbers.IsArray() || !primary_docs.IsArray())
                return {};

            std::vector<FilingItem> filings;
            const size_t count = std::min({ forms.array_value.size(), dates.array_value.size(), accession_numbers.array_value.size(), primary_docs.array_value.size() });
            for (size_t i = 0; i < count; ++i)
            {
                const std::string form = forms.array_value[i].AsString();
                const std::string upper_form = Upper(form);
                const bool keep = upper_form.find("10-K") == 0 || upper_form.find("10-Q") == 0 || upper_form.find("8-K") == 0 || upper_form == "4" || upper_form == "3" || upper_form == "5";
                if (!keep)
                    continue;

                const std::string accession = accession_numbers.array_value[i].AsString();
                std::string accession_dir = accession;
                accession_dir.erase(std::remove(accession_dir.begin(), accession_dir.end(), '-'), accession_dir.end());
                const std::string primary_doc = primary_docs.array_value[i].AsString();

                FilingItem item;
                item.form = form;
                item.date = dates.array_value[i].AsString();
                item.title = symbol + " " + form + " filing";
                item.url = "https://www.sec.gov/Archives/edgar/data/" + UnpaddedCik(cik) + "/" + accession_dir + "/" + primary_doc;
                item.summary = FilingSummaryForForm(form);
                item.risk_change = FilingRiskReadForForm(form);
                filings.push_back(std::move(item));
                if (filings.size() >= 12)
                    break;
            }
            return filings;
        }
    }

    std::vector<Candle> BuildSyntheticCandles(const StockQuote& quote, int days)
    {
        std::vector<Candle> candles;
        const int count = std::max(60, days);
        candles.reserve(static_cast<size_t>(count));
        const double seed = SeedValue(quote.symbol);
        const double current = quote.price > 0.0 ? quote.price : 100.0;
        const double trend = Clamp(quote.change_percent / 100.0, -0.04, 0.04);
        double close = current * (1.0 - trend * 4.0 - 0.10 + seed * 0.20);

        for (int i = 0; i < count; ++i)
        {
            const double t = static_cast<double>(i) / static_cast<double>(count - 1);
            const double drift = (current - close) * 0.025 + current * trend * 0.12;
            const double wave = std::sin(t * 18.0 + seed * 6.0) * current * (0.003 + seed * 0.004);
            const double noise = std::sin((i + 3) * (1.7 + seed)) * current * 0.0025;
            const double open = close;
            close = std::max(1.0, close + drift + wave + noise);
            const double range = current * (0.010 + seed * 0.012 + std::fabs(std::sin(i * 0.37)) * 0.012);
            Candle candle;
            candle.date = DemoDate(count - i);
            candle.open = open;
            candle.close = close;
            candle.high = std::max(open, close) + range * 0.55;
            candle.low = std::max(0.01, std::min(open, close) - range * 0.45);
            candle.volume = static_cast<long long>(std::max(100000.0, static_cast<double>(quote.volume > 0 ? quote.volume : 5000000) * (0.65 + seed * 0.4 + std::fabs(std::sin(i * 0.21)) * 0.45)));
            candles.push_back(candle);
        }

        candles.back().close = current;
        candles.back().open = quote.open > 0.0 ? quote.open : candles.back().open;
        candles.back().high = quote.high > 0.0 ? quote.high : std::max(candles.back().open, candles.back().close) * 1.01;
        candles.back().low = quote.low > 0.0 ? quote.low : std::min(candles.back().open, candles.back().close) * 0.99;
        candles.back().volume = quote.volume > 0 ? quote.volume : candles.back().volume;
        return candles;
    }

    HistoricalCandlesResult LoadHistoricalCandles(const Config& config, const StockQuote& quote, int days)
    {
        HistoricalCandlesResult result;
        const std::string symbol = Upper(quote.symbol);
        std::vector<Candle> cached = LoadCachedCandles(symbol, days);
        const std::string key = ConfiguredAlphaKey(config);
        const std::filesystem::path cache_path = HistoryCacheFile(symbol);
        result.source = "Alpha Vantage TIME_SERIES_DAILY";
        result.fetched_at = NowTimeLabel();

        if (key.empty())
        {
            result.candles = !cached.empty() ? std::move(cached) : BuildSyntheticCandles(quote, days);
            result.ok = !result.candles.empty();
            result.cached = !cached.empty();
            result.source = result.cached ? "Local history cache" : "Generated demo candles";
            result.cache_age_label = result.cached ? "cache used; no API key" : "no cache";
            result.fallback_reason = result.cached ? "No Alpha Vantage key configured." : "No Alpha Vantage key configured and no cache file was available.";
            result.status = result.cached
                ? symbol + " daily candles loaded from local cache."
                : symbol + " daily candles generated because no Alpha Vantage key is configured.";
            return result;
        }

        if (!config.force_live_refresh && !cached.empty() && IsCacheFresh(cache_path, config.history_cache_hours))
        {
            result.ok = true;
            result.cached = true;
            result.candles = std::move(cached);
            result.source = "Local history cache";
            result.cache_age_label = "fresh within " + std::to_string(std::max(1, config.history_cache_hours)) + " hour TTL";
            result.status = symbol + " daily candles loaded from fresh local cache.";
            return result;
        }

        const std::string url = "https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol="
            + UrlEncode(symbol) + "&outputsize=full&apikey=" + UrlEncode(key);
        const HttpResponse response = HttpGet(url);
        if (!response.error.empty() || response.status_code < 200 || response.status_code >= 300)
        {
            result.candles = !cached.empty() ? std::move(cached) : BuildSyntheticCandles(quote, days);
            result.ok = !result.candles.empty();
            result.cached = !cached.empty();
            result.source = result.cached ? "Local history cache" : "Generated demo candles";
            result.cache_age_label = result.cached ? "stale cache fallback" : "no cache";
            result.status = response.error.empty()
                ? symbol + " history request returned HTTP " + std::to_string(response.status_code) + "; using fallback candles."
                : symbol + " history request failed: " + response.error + "; using fallback candles.";
            result.fallback_reason = result.status;
            AppendDiagnosticLine("history " + symbol + " " + result.status);
            return result;
        }

        const JsonParseResult parsed = ParseJson(response.body);
        if (!parsed.ok)
        {
            result.candles = !cached.empty() ? std::move(cached) : BuildSyntheticCandles(quote, days);
            result.ok = !result.candles.empty();
            result.cached = !cached.empty();
            result.source = result.cached ? "Local history cache" : "Generated demo candles";
            result.cache_age_label = result.cached ? "parse fallback cache" : "parse fallback synthetic";
            result.status = symbol + " history parse failed; using fallback candles.";
            result.fallback_reason = "Alpha Vantage JSON parse failed: " + parsed.error;
            AppendDiagnosticLine("history " + symbol + " parse failed " + parsed.error);
            return result;
        }

        const std::string note = parsed.value["Note"].AsString(parsed.value["Information"].AsString(parsed.value["Error Message"].AsString()));
        std::vector<Candle> live = ParseAlphaDailyCandles(parsed.value, days);
        if (live.empty())
        {
            result.candles = !cached.empty() ? std::move(cached) : BuildSyntheticCandles(quote, days);
            result.ok = !result.candles.empty();
            result.cached = !cached.empty();
            result.source = result.cached ? "Local history cache" : "Generated demo candles";
            result.cache_age_label = result.cached ? "provider-limit cache fallback" : "provider-limit synthetic fallback";
            result.status = !note.empty()
                ? symbol + " history limited by provider: " + note
                : symbol + " history returned no daily candles; using fallback candles.";
            result.fallback_reason = result.status;
            AppendDiagnosticLine("history " + symbol + " " + result.status);
            return result;
        }

        SaveCachedCandles(symbol, live);
        result.ok = true;
        result.live = true;
        result.candles = std::move(live);
        result.cache_age_label = "live fetch";
        result.status = symbol + " daily candles synced from Alpha Vantage TIME_SERIES_DAILY.";
        return result;
    }

    ResearchBundleResult LoadResearchBundle(const Config& config, const StockQuote& quote)
    {
        const std::string symbol = Upper(quote.symbol);
        const std::string key = ConfiguredAlphaKey(config);
        ResearchBundleResult cached = LoadResearchCache(quote);
        const std::filesystem::path cache_path = ResearchCacheFile(symbol);
        cached.fetched_at = NowTimeLabel();
        cached.source = cached.source.empty() ? "Local research cache" : cached.source;

        if (key.empty())
        {
            if (cached.ok)
            {
                cached.cache_age_label = "cache used; no API key";
                cached.fallback_reason = "No Alpha Vantage key configured.";
                cached.status = symbol + " research bundle loaded from local cache.";
                return cached;
            }
            ResearchBundleResult local;
            local.ok = true;
            local.source = "SEC EDGAR plus generated local models";
            local.fetched_at = NowTimeLabel();
            local.cache_age_label = "no Alpha Vantage key";
            local.fundamentals = BuildFundamentals(quote);
            local.filings = LoadSecFilings(symbol);
            const bool sec_live = !local.filings.empty();
            if (!sec_live)
                local.filings = BuildFilings(symbol);
            local.news = BuildNews(symbol);
            local.earnings = BuildEarnings(symbol);
            local.status = sec_live
                ? symbol + " SEC filings synced; remaining research generated because no Alpha Vantage key is configured."
                : symbol + " research bundle generated because no Alpha Vantage key is configured.";
            local.fallback_reason = sec_live ? "Alpha Vantage key missing; SEC data used where possible." : "No Alpha Vantage key configured and SEC returned no recent filings.";
            if (sec_live)
                SaveResearchCache(symbol, local);
            return local;
        }

        if (!config.force_live_refresh && cached.ok && IsCacheFresh(cache_path, config.research_cache_hours))
        {
            cached.cache_age_label = "fresh within " + std::to_string(std::max(1, config.research_cache_hours)) + " hour TTL";
            cached.status = symbol + " research bundle loaded from fresh local cache.";
            return cached;
        }

        ResearchBundleResult live;
        live.source = "Alpha Vantage + SEC EDGAR";
        live.fetched_at = NowTimeLabel();
        live.fundamentals = cached.fundamentals.symbol.empty() ? BuildFundamentals(quote) : cached.fundamentals;
        live.filings = cached.filings.empty() ? BuildFilings(symbol) : cached.filings;
        live.news = cached.news.empty() ? BuildNews(symbol) : cached.news;
        live.earnings = cached.earnings.empty() ? BuildEarnings(symbol) : cached.earnings;

        int synced = 0;
        std::vector<std::string> warnings;
        const auto fetch_json = [&](const std::string& url, const std::string& endpoint, JsonValue& out) {
            const HttpResponse response = HttpGet(url);
            if (!response.error.empty())
            {
                warnings.push_back(endpoint + ": " + response.error);
                return false;
            }
            if (response.status_code < 200 || response.status_code >= 300)
            {
                warnings.push_back(endpoint + ": HTTP " + std::to_string(response.status_code));
                return false;
            }
            const JsonParseResult parsed = ParseJson(response.body);
            if (!parsed.ok)
            {
                warnings.push_back(endpoint + ": parse failed");
                return false;
            }
            const std::string note = ProviderNote(parsed.value);
            if (!note.empty())
            {
                warnings.push_back(endpoint + ": " + note);
                return false;
            }
            out = parsed.value;
            return true;
        };

        JsonValue overview;
        const std::string overview_url = "https://www.alphavantage.co/query?function=OVERVIEW&symbol=" + UrlEncode(symbol) + "&apikey=" + UrlEncode(key);
        if (fetch_json(overview_url, "OVERVIEW", overview) && overview.IsObject() && overview.Has("Symbol"))
        {
            live.fundamentals = ParseAlphaOverview(quote, overview);
            ++synced;
        }

        std::vector<FilingItem> sec_filings = LoadSecFilings(symbol);
        if (!sec_filings.empty())
        {
            live.filings = std::move(sec_filings);
            ++synced;
        }
        else
        {
            warnings.push_back("SEC EDGAR: no recent filings returned");
        }

        JsonValue news;
        const std::string news_url = "https://www.alphavantage.co/query?function=NEWS_SENTIMENT&tickers=" + UrlEncode(symbol) + "&sort=LATEST&limit=20&apikey=" + UrlEncode(key);
        if (fetch_json(news_url, "NEWS_SENTIMENT", news))
        {
            std::vector<NewsItem> parsed_news = ParseAlphaNews(symbol, news);
            if (!parsed_news.empty())
            {
                live.news = std::move(parsed_news);
                ++synced;
            }
        }

        JsonValue earnings;
        const std::string earnings_url = "https://www.alphavantage.co/query?function=EARNINGS&symbol=" + UrlEncode(symbol) + "&apikey=" + UrlEncode(key);
        if (fetch_json(earnings_url, "EARNINGS", earnings))
        {
            std::vector<EarningsItem> parsed_earnings = ParseAlphaEarnings(earnings);
            if (!parsed_earnings.empty())
            {
                live.earnings = std::move(parsed_earnings);
                ++synced;
            }
        }

        live.ok = true;
        live.live = synced > 0;
        live.cached = !cached.ok ? false : synced < 3;
        if (synced > 0)
        {
            live.status = symbol + " research bundle synced (" + std::to_string(synced) + "/4 provider feeds).";
            SaveResearchCache(symbol, live);
        }
        else if (cached.ok)
        {
            cached.status = warnings.empty()
                ? symbol + " research provider returned no usable data; cache loaded."
                : symbol + " research provider limited; cache loaded. " + warnings.front();
            cached.cache_age_label = "provider fallback cache";
            cached.fallback_reason = warnings.empty() ? "Provider returned no usable data." : warnings.front();
            AppendDiagnosticLine("research " + symbol + " " + cached.status);
            return cached;
        }
        else
        {
            live.status = warnings.empty()
                ? symbol + " research bundle generated; provider returned no usable data."
                : symbol + " research bundle generated after provider warning: " + warnings.front();
            live.source = "Generated local research";
            live.cache_age_label = "provider fallback generated";
            live.fallback_reason = warnings.empty() ? "Provider returned no usable data." : warnings.front();
            AppendDiagnosticLine("research " + symbol + " " + live.status);
        }
        return live;
    }

    IndicatorSnapshot BuildIndicators(const std::vector<Candle>& candles, const StockQuote* spy, const StockQuote* qqq)
    {
        IndicatorSnapshot snapshot;
        if (candles.empty())
            return snapshot;

        std::vector<double> closes;
        closes.reserve(candles.size());
        for (const Candle& candle : candles)
            closes.push_back(candle.close);

        snapshot.sma20 = LastSma(closes, 20);
        snapshot.sma50 = LastSma(closes, 50);
        snapshot.ema12 = LastEma(closes, 12);
        snapshot.ema26 = LastEma(closes, 26);
        const std::vector<double> ema12 = EmaSeries(closes, 12);
        const std::vector<double> ema26 = EmaSeries(closes, 26);
        std::vector<double> macd_series;
        for (size_t i = 0; i < closes.size(); ++i)
            macd_series.push_back(ema12[i] - ema26[i]);
        snapshot.macd = macd_series.empty() ? 0.0 : macd_series.back();
        snapshot.macd_signal = LastEma(macd_series, 9);
        snapshot.macd_histogram = snapshot.macd - snapshot.macd_signal;

        double gains = 0.0;
        double losses = 0.0;
        const size_t start = closes.size() > 15 ? closes.size() - 14 : 1;
        for (size_t i = start; i < closes.size(); ++i)
        {
            const double change = closes[i] - closes[i - 1];
            if (change >= 0.0)
                gains += change;
            else
                losses -= change;
        }
        const double rs = losses <= 0.0 ? 100.0 : gains / losses;
        snapshot.rsi14 = 100.0 - (100.0 / (1.0 + rs));

        snapshot.bollinger_mid = snapshot.sma20;
        const size_t bb_start = closes.size() >= 20 ? closes.size() - 20 : 0;
        double variance = 0.0;
        for (size_t i = bb_start; i < closes.size(); ++i)
            variance += std::pow(closes[i] - snapshot.bollinger_mid, 2.0);
        const double stdev = std::sqrt(variance / std::max<size_t>(1, closes.size() - bb_start));
        snapshot.bollinger_upper = snapshot.bollinger_mid + stdev * 2.0;
        snapshot.bollinger_lower = snapshot.bollinger_mid - stdev * 2.0;

        double tr_sum = 0.0;
        int tr_count = 0;
        const size_t atr_start = candles.size() > 15 ? candles.size() - 14 : 1;
        for (size_t i = atr_start; i < candles.size(); ++i)
        {
            const double prev_close = candles[i - 1].close;
            const double tr = std::max(candles[i].high - candles[i].low, std::max(std::fabs(candles[i].high - prev_close), std::fabs(candles[i].low - prev_close)));
            tr_sum += tr;
            ++tr_count;
        }
        snapshot.atr14 = tr_count > 0 ? tr_sum / static_cast<double>(tr_count) : 0.0;

        const double peak = *std::max_element(closes.begin(), closes.end());
        snapshot.drawdown = peak > 0.0 ? ((closes.back() - peak) / peak) * 100.0 : 0.0;
        const double first = closes.front();
        const double own_return = first > 0.0 ? ((closes.back() - first) / first) * 100.0 : 0.0;
        snapshot.relative_spy = own_return - (spy ? spy->change_percent : 0.0);
        snapshot.relative_qqq = own_return - (qqq ? qqq->change_percent : 0.0);

        std::vector<double> returns;
        for (size_t i = 1; i < closes.size(); ++i)
        {
            if (closes[i - 1] > 0.0)
                returns.push_back((closes[i] - closes[i - 1]) / closes[i - 1]);
        }
        const double mean = returns.empty() ? 0.0 : std::accumulate(returns.begin(), returns.end(), 0.0) / static_cast<double>(returns.size());
        double var = 0.0;
        for (double r : returns)
            var += std::pow(r - mean, 2.0);
        snapshot.volatility = returns.empty() ? 0.0 : std::sqrt(var / returns.size()) * std::sqrt(252.0) * 100.0;

        if (snapshot.sma20 > snapshot.sma50 && snapshot.macd_histogram > 0.0 && snapshot.rsi14 < 72.0)
            snapshot.regime = "Bullish";
        else if (snapshot.sma20 < snapshot.sma50 && snapshot.macd_histogram < 0.0)
            snapshot.regime = "Risk-off";
        else if (snapshot.volatility > 45.0)
            snapshot.regime = "High volatility";
        else
            snapshot.regime = "Choppy";
        return snapshot;
    }

    BacktestResult RunSignalBacktest(const std::vector<Candle>& candles, const std::string& preset)
    {
        BacktestResult result;
        if (candles.size() < 55)
            return result;

        double gross_win = 0.0;
        double gross_loss = 0.0;
        double equity = 1.0;
        double peak = 1.0;
        for (size_t i = 50; i + 6 < candles.size(); i += 5)
        {
            std::vector<double> closes;
            for (size_t j = i - 50; j <= i; ++j)
                closes.push_back(candles[j].close);
            const double sma20 = LastSma(closes, 20);
            const double sma50 = LastSma(closes, 50);
            const bool enter = preset == "Oversold bounce"
                ? candles[i].close < sma20 * 0.98
                : preset == "Breakout"
                    ? candles[i].close > sma20 * 1.015
                    : sma20 >= sma50;
            if (!enter)
                continue;

            const double ret = candles[i].close > 0.0 ? ((candles[i + 5].close - candles[i].close) / candles[i].close) * 100.0 : 0.0;
            ++result.trades;
            result.average_return += ret;
            result.best_trade = result.trades == 1 ? ret : std::max(result.best_trade, ret);
            result.worst_trade = result.trades == 1 ? ret : std::min(result.worst_trade, ret);
            equity *= 1.0 + (ret / 100.0);
            peak = std::max(peak, equity);
            result.max_drawdown = std::min(result.max_drawdown, peak > 0.0 ? ((equity - peak) / peak) * 100.0 : 0.0);
            if (ret >= 0.0)
            {
                ++result.wins;
                gross_win += ret;
            }
            else
            {
                ++result.losses;
                gross_loss += std::fabs(ret);
            }
        }

        if (result.trades > 0)
        {
            result.average_return /= static_cast<double>(result.trades);
            result.win_rate = static_cast<double>(result.wins) * 100.0 / static_cast<double>(result.trades);
            result.profit_factor = gross_loss > 0.0 ? gross_win / gross_loss : gross_win;
        }
        result.rows = {
            {"Trades", "", std::to_string(result.trades), "", "Number of historical setup samples in the synthetic/test candle set."},
            {"Win rate", "", FormatPercent(result.win_rate), "", "Percent of samples with positive 5-bar forward return."},
            {"Average return", "", FormatPercent(result.average_return), "", "Average 5-bar forward return across setup samples."},
            {"Max drawdown", "", FormatPercent(result.max_drawdown), "", "Worst equity curve drawdown during the test."},
            {"Profit factor", "", std::to_string(static_cast<int>(result.profit_factor * 100.0) / 100.0), "", "Gross winning returns divided by gross losing returns."}
        };
        return result;
    }

    FundamentalSnapshot BuildFundamentals(const StockQuote& quote)
    {
        const double seed = SeedValue(quote.symbol);
        FundamentalSnapshot f;
        f.symbol = quote.symbol;
        f.revenue = 25000000000.0 + seed * 420000000000.0;
        f.eps = 1.2 + seed * 14.0;
        f.gross_margin = 22.0 + seed * 48.0;
        f.net_margin = 8.0 + seed * 28.0;
        f.debt_to_equity = 0.15 + seed * 1.9;
        f.free_cash_flow = 1500000000.0 + seed * 85000000000.0;
        f.pe = quote.pe_ratio;
        f.peg = 0.8 + seed * 2.3;
        f.dividend_yield = quote.dividend_yield;
        f.summary = quote.symbol + " fundamental card is provider-ready. Connect Alpha Vantage fundamentals, SEC companyfacts, or another provider for reported values.";
        return f;
    }

    std::vector<FilingItem> BuildFilings(const std::string& symbol)
    {
        return {
            {"10-K", "Recent", symbol + " annual report", "https://www.sec.gov/edgar/search/", "Annual business, risk, MD&A, and financial statement review.", "Risk factors: compare latest filing against prior year."},
            {"10-Q", "Recent", symbol + " quarterly report", "https://www.sec.gov/edgar/search/", "Quarterly operating update and liquidity snapshot.", "Watch for margin, debt, or guidance language changes."},
            {"8-K", "Monitor", symbol + " current reports", "https://www.sec.gov/edgar/search/", "Material company events, presentations, agreements, and leadership changes.", "Event-driven risk review required."},
            {"Form 4", "Monitor", symbol + " insider transactions", "https://www.sec.gov/edgar/search/", "Insider buying/selling signal feed.", "Cluster buys deserve a manual check."}
        };
    }

    std::vector<NewsItem> BuildNews(const std::string& symbol)
    {
        const double seed = SeedValue(symbol);
        return {
            {"Today", "Aegis News", symbol + " momentum brief", seed > 0.55 ? "Positive" : "Neutral", "News/sentiment provider slot. Alpha Vantage NEWS_SENTIMENT can populate this feed.", "https://www.alphavantage.co/documentation/"},
            {"Today", "Market Desk", "Sector read-through for " + symbol, seed > 0.35 ? "Neutral" : "Negative", "Compare company move against sector and index peers before acting.", ""},
            {"This week", "Research Queue", symbol + " catalyst watch", "Watch", "Earnings, filings, macro releases, and unusual volume are treated as catalyst candidates.", ""}
        };
    }

    std::vector<EarningsItem> BuildEarnings(const std::string& symbol)
    {
        const double seed = SeedValue(symbol);
        return {
            {"Next", "Upcoming", 1.0 + seed * 4.0, 0.0, 0.0, 3.5 + seed * 5.0, 0.0},
            {"Prior", "Reported", 0.9 + seed * 3.8, 1.0 + seed * 4.2, -4.0 + seed * 12.0, 0.0, -2.0 + seed * 8.0}
        };
    }

    OptionSnapshot BuildOptionSnapshot(const StockQuote& quote)
    {
        const double seed = SeedValue(quote.symbol);
        return { 24.0 + seed * 42.0, quote.price * (0.025 + seed * 0.055), 0.65 + seed * 0.95, -8.0 + seed * 18.0, seed > 0.72 ? "Call volume elevated" : "Normal flow" };
    }

    std::vector<ProviderStatus> BuildProviderStatuses(const Config& config)
    {
        return {
            {"Alpha Vantage", Trim(config.alpha_vantage_api_key).empty() ? "Needs key" : "Configured", "Quotes, fundamentals, technical indicators, news, earnings, dividends.", "https://www.alphavantage.co/documentation/", !Trim(config.alpha_vantage_api_key).empty()},
            {"SEC EDGAR", "Ready", "Company submissions and XBRL companyfacts are public REST JSON feeds.", "https://www.sec.gov/edgar/sec-api-documentation", true},
            {"Alpaca Paper", "Disabled", "Paper trading/account sync slot. Live trading remains disabled by design.", "https://docs.alpaca.markets/docs/trading/paper-trading/", false},
            {"FRED/Macro", "Provider slot", "Rates, CPI, unemployment, yield curve, and macro dashboard inputs.", "https://fred.stlouisfed.org/docs/api/fred/", false},
            {"OpenBB-style layer", "Designed", "Provider abstraction point for swapping quote/fundamental/news sources.", "https://docs.openbb.co/", false}
        };
    }

    std::vector<MacroItem> BuildMacroDashboard()
    {
        return {
            {"Market regime", "Mixed", "Choppy", "Derived from index momentum, volatility, and breadth provider slots."},
            {"Rates", "Watch", "Higher", "FRED provider slot for Treasury and Fed funds curves."},
            {"Inflation", "Monitor", "Cooling", "CPI/PCE trend can temper equity multiple assumptions."},
            {"Dollar", "Neutral", "Range", "Dollar strength can pressure multinationals and commodities."},
            {"Credit", "Stable", "Flat", "Credit stress would lower risk appetite in the model."}
        };
    }

    std::vector<WatchlistGroup> BuildWatchlistGroups(const std::vector<std::string>& symbols)
    {
        WatchlistGroup long_term{ "Long-term", {} };
        WatchlistGroup swing{ "Swing", {} };
        WatchlistGroup earnings{ "Earnings", {} };
        WatchlistGroup etfs{ "ETFs", {} };
        WatchlistGroup high_risk{ "High risk", {} };
        WatchlistGroup dividend{ "Dividend", {} };
        WatchlistGroup ai_semis{ "AI/Semis", {} };
        WatchlistGroup energy{ "Energy", {} };
        for (const std::string& symbol : symbols)
        {
            const std::string s = Upper(symbol);
            if (s == "SPY" || s == "QQQ")
                etfs.symbols.push_back(s);
            if (s == "NVDA" || s == "AMD" || s == "AVGO" || s == "MSFT" || s == "GOOGL")
                ai_semis.symbols.push_back(s);
            if (s == "TSLA" || s == "AMD")
                high_risk.symbols.push_back(s);
            if (s == "XOM")
                energy.symbols.push_back(s);
            if (s == "AAPL" || s == "MSFT" || s == "COST")
                long_term.symbols.push_back(s);
            if (s == "JPM" || s == "V" || s == "XOM" || s == "COST")
                dividend.symbols.push_back(s);
            swing.symbols.push_back(s);
            if (SeedValue(s) > 0.55)
                earnings.symbols.push_back(s);
        }
        return { long_term, swing, earnings, etfs, high_risk, dividend, ai_semis, energy };
    }

    std::vector<ScreenPreset> BuildScreenPresets()
    {
        return {
            {"Momentum", "Strong price tape with positive moving-average posture.", "momentum"},
            {"Value", "Fundamental valuation and cash-flow watchlist.", "value"},
            {"Oversold bounce", "Mean-reversion candidates below short-term trend.", "oversold"},
            {"Breakout", "New range highs with volume expansion.", "breakout"},
            {"Defensive", "Lower volatility, dividend, and stable sector posture.", "defensive"},
            {"Dividend", "Yield and payout candidates for income research.", "dividend"}
        };
    }

    std::vector<FocusItem> BuildFocusItems(const StockState& state, const std::vector<PriceAlert>& alerts)
    {
        std::vector<FocusItem> items;
        for (const StockQuote& quote : state.quotes)
        {
            if (std::fabs(quote.change_percent) >= 1.5)
                items.push_back({ quote.symbol, quote.change_percent > 0.0 ? "Price strength" : "Price weakness", "High", FormatPercent(quote.change_percent) + " move from previous close." });
        }
        for (const PriceAlert& alert : alerts)
        {
            if (alert.enabled)
                items.push_back({ alert.symbol, "Custom alert armed", "Watch", std::string(alert.above ? "Above " : "Below ") + FormatCurrency(alert.trigger_price) });
        }
        for (const StockSignal& signal : state.signals)
        {
            if (signal.confidence >= 70)
                items.push_back({ signal.symbol, "High conviction signal", "High", signal.rating + " at " + std::to_string(signal.confidence) + "% confidence." });
        }
        std::sort(items.begin(), items.end(), [](const FocusItem& a, const FocusItem& b) {
            return a.priority < b.priority;
        });
        return items;
    }

    std::vector<EtfExposure> BuildEtfExposure(const std::string& symbol)
    {
        const double seed = SeedValue(symbol);
        return {
            {"SPY", 0.1 + seed * 7.2, "Broad S&P 500 exposure estimate."},
            {"QQQ", 0.2 + seed * 8.8, "Growth-heavy Nasdaq exposure estimate."},
            {"Sector ETF", 0.4 + seed * 6.0, "Sector basket exposure estimate."}
        };
    }

    RiskSummary BuildRiskSummary(const StockState& state, const std::vector<PortfolioHolding>& holdings, double cash)
    {
        RiskSummary summary;
        double value = 0.0;
        double weighted_beta = 0.0;
        double largest = 0.0;
        for (const PortfolioHolding& holding : holdings)
        {
            const StockQuote* quote = nullptr;
            for (const StockQuote& row : state.quotes)
            {
                if (Upper(row.symbol) == Upper(holding.symbol))
                {
                    quote = &row;
                    break;
                }
            }
            const double mark = quote != nullptr && quote->price > 0.0 ? quote->price : holding.average_cost;
            const double row_value = mark * holding.shares;
            value += row_value;
            weighted_beta += row_value * (quote != nullptr ? quote->beta : 1.0);
            largest = std::max(largest, row_value);
        }
        summary.beta_exposure = value > 0.0 ? weighted_beta / value : 0.0;
        summary.cash_drag = (value + cash) > 0.0 ? (cash / (value + cash)) * 100.0 : 0.0;
        summary.concentration = value > 0.0 ? (largest / value) * 100.0 : 0.0;
        summary.estimated_drawdown = -(8.0 + summary.beta_exposure * 9.0 + std::max(0.0, summary.concentration - 20.0) * 0.25);
        summary.rows = {
            {"Beta exposure", "", std::to_string(static_cast<int>(summary.beta_exposure * 100.0) / 100.0), "", "Weighted beta estimate across saved holdings."},
            {"Estimated drawdown", "", FormatPercent(summary.estimated_drawdown), "", "Scenario estimate based on beta and concentration."},
            {"Cash drag", "", FormatPercent(summary.cash_drag), "", "Cash as a percent of paper portfolio value plus cash."},
            {"Concentration", "", FormatPercent(summary.concentration), "", "Largest saved holding as a percent of marked holdings."}
        };
        return summary;
    }

    std::vector<InfoItem> BuildTradeChecklist(const StockQuote& quote, const StockSignal& signal, const IndicatorSnapshot& indicators)
    {
        return {
            {"Trend", "", indicators.sma20 >= indicators.sma50 ? "Pass" : "Review", "", "20-day average is " + std::string(indicators.sma20 >= indicators.sma50 ? "above" : "below") + " the 50-day average."},
            {"Volume", "", quote.volume > 10000000 ? "Pass" : "Thin", "", "Volume check before saving a plan."},
            {"Catalyst", "", "Manual", "", "Confirm earnings, news, filing, or macro catalyst."},
            {"Risk/reward", "", signal.upside, "", "Target, stop, and position size should produce acceptable asymmetry."},
            {"Earnings date", "", "Check", "", "Avoid sizing blindly into earnings unless that is the intended setup."},
            {"Market regime", "", indicators.regime, "", "Regime should align with strategy preset."}
        };
    }

    std::vector<InfoItem> BuildModelExplanation(const StockQuote& quote, const StockSignal& signal, const IndicatorSnapshot& indicators)
    {
        return {
            {"Price momentum", "", FormatPercent(quote.change_percent), std::to_string(signal.score), "Latest move and trend alignment affect conviction."},
            {"Trend stack", "", indicators.sma20 >= indicators.sma50 ? "Positive" : "Negative", "", "SMA20/SMA50 relationship is a core model input."},
            {"RSI", "", std::to_string(static_cast<int>(indicators.rsi14)), "", "Overbought/oversold conditions temper the score."},
            {"MACD", "", FormatCurrency(indicators.macd_histogram), "", "Positive histogram supports bullish momentum."},
            {"Risk", "", signal.risk, "", "Volatility, ATR, beta, and range reduce aggressive sizing."}
        };
    }

    std::vector<InfoItem> BuildRiskCritic(const StockQuote& quote, const StockSignal& signal, const IndicatorSnapshot& indicators)
    {
        return {
            {"Invalidation", "", FormatCurrency(signal.stop_level), "", "Break below stop or thesis level invalidates the setup."},
            {"Volatility", "", FormatPercent(indicators.volatility), "", "High volatility can turn good direction into bad execution."},
            {"Drawdown", "", FormatPercent(indicators.drawdown), "", "Deep drawdown may signal broken trend or opportunity; verify manually."},
            {"News risk", "", "Manual", "", "Check fresh headlines and filings before acting."},
            {"Liquidity", "", FormatVolume(quote.volume), "", "Low volume increases slippage and false-breakout risk."}
        };
    }

    std::vector<InfoItem> BuildSimilarSetups(const StockQuote& quote, const IndicatorSnapshot& indicators)
    {
        return {
            {quote.symbol + " / prior momentum", "", indicators.regime, "", "Similar setup bucket based on trend and MACD state."},
            {"Index relative strength", "", FormatPercent(indicators.relative_spy), "", "Compare against SPY to avoid confusing market beta with stock alpha."},
            {"Growth peer basket", "", FormatPercent(indicators.relative_qqq), "", "QQQ comparison helps classify tech/growth setups."}
        };
    }

    std::vector<InfoItem> BuildModelAudit(const std::vector<TradePlan>& plans, const StockState& state)
    {
        std::vector<InfoItem> rows;
        int advancing = 0;
        int stopped = 0;
        int targets = 0;
        for (const TradePlan& plan : plans)
        {
            const StockQuote* quote = nullptr;
            for (const StockQuote& row : state.quotes)
            {
                if (Upper(row.symbol) == Upper(plan.symbol))
                {
                    quote = &row;
                    break;
                }
            }
            const double mark = quote != nullptr ? quote->price : plan.entry;
            if (mark >= plan.target && plan.target > 0.0)
                ++targets;
            else if (mark <= plan.stop && plan.stop > 0.0)
                ++stopped;
            else if (mark >= plan.entry)
                ++advancing;
        }
        rows.push_back({"Plans tracked", "", std::to_string(static_cast<int>(plans.size())), "", "Saved paper plans under model audit."});
        rows.push_back({"Advancing", "", std::to_string(advancing), "", "Plans currently above entry but below target."});
        rows.push_back({"Target hit", "", std::to_string(targets), "", "Plans at or above target."});
        rows.push_back({"Stop hit", "", std::to_string(stopped), "", "Plans at or below stop."});
        return rows;
    }

    std::vector<SymbolNote> LoadSymbolNotes()
    {
        std::vector<SymbolNote> notes;
        std::ifstream file(NotesFile());
        if (!file)
            return notes;
        std::string line;
        while (std::getline(file, line))
        {
            const std::vector<std::string> parts = SplitTabs(line);
            if (parts.size() < 4)
                continue;
            notes.push_back({ Upper(Trim(parts[0])), Trim(parts[1]), Trim(parts[2]), Trim(parts[3]) });
        }
        return notes;
    }

    bool SaveSymbolNotes(const std::vector<SymbolNote>& notes)
    {
        std::error_code ec;
        std::filesystem::create_directories(AppDataDirectory(), ec);
        std::ofstream file(NotesFile(), std::ios::trunc);
        if (!file)
            return false;
        for (const SymbolNote& note : notes)
        {
            if (Trim(note.symbol).empty())
                continue;
            file << TsvField(Upper(note.symbol)) << '\t'
                 << TsvField(note.tags) << '\t'
                 << TsvField(note.note) << '\t'
                 << TsvField(note.updated_at.empty() ? NowTimeLabel() : note.updated_at) << '\n';
        }
        return true;
    }

    std::vector<JournalEntry> LoadJournalEntries()
    {
        std::vector<JournalEntry> entries;
        std::ifstream file(JournalFile());
        if (!file)
            return entries;
        std::string line;
        while (std::getline(file, line))
        {
            const std::vector<std::string> parts = SplitTabs(line);
            if (parts.size() < 9)
                continue;
            entries.push_back({ Trim(parts[0]), Upper(Trim(parts[1])), Trim(parts[2]), Trim(parts[3]), Trim(parts[4]), Trim(parts[5]), Trim(parts[6]), Trim(parts[7]), ParseDouble(parts[8]) });
        }
        return entries;
    }

    bool SaveJournalEntries(const std::vector<JournalEntry>& entries)
    {
        std::error_code ec;
        std::filesystem::create_directories(AppDataDirectory(), ec);
        std::ofstream file(JournalFile(), std::ios::trunc);
        if (!file)
            return false;
        for (const JournalEntry& entry : entries)
        {
            file << TsvField(entry.time.empty() ? NowTimeLabel() : entry.time) << '\t'
                 << TsvField(Upper(entry.symbol)) << '\t'
                 << TsvField(entry.action) << '\t'
                 << TsvField(entry.reason) << '\t'
                 << TsvField(entry.exit_reason) << '\t'
                 << TsvField(entry.tags) << '\t'
                 << TsvField(entry.mistakes) << '\t'
                 << TsvField(entry.grade) << '\t'
                 << entry.realized_pnl << '\n';
        }
        return true;
    }
}
