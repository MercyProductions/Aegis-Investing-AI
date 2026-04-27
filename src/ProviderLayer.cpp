#include "ProviderLayer.h"

#include "Diagnostics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace aegis
{
    namespace
    {
        std::string BoolLabel(bool value)
        {
            return value ? "Enabled" : "Needs setup";
        }

        InfoItem Row(std::string name, std::string state, std::string value, std::string detail, std::string source = {})
        {
            InfoItem item;
            item.name = std::move(name);
            item.state = std::move(state);
            item.value = std::move(value);
            item.detail = std::move(detail);
            item.source = std::move(source);
            return item;
        }

        std::vector<double> Returns(const std::vector<Candle>& candles)
        {
            std::vector<double> returns;
            if (candles.size() < 2)
                return returns;
            returns.reserve(candles.size() - 1);
            for (size_t i = 1; i < candles.size(); ++i)
            {
                if (candles[i - 1].close > 0.0)
                    returns.push_back((candles[i].close - candles[i - 1].close) / candles[i - 1].close);
            }
            return returns;
        }

        double Average(const std::vector<double>& values)
        {
            if (values.empty())
                return 0.0;
            double sum = 0.0;
            for (double value : values)
                sum += value;
            return sum / static_cast<double>(values.size());
        }

        double StdDev(const std::vector<double>& values)
        {
            if (values.size() < 2)
                return 0.0;
            const double avg = Average(values);
            double sum = 0.0;
            for (double value : values)
                sum += (value - avg) * (value - avg);
            return std::sqrt(sum / static_cast<double>(values.size() - 1));
        }

        bool EvaluateSingleRule(const Candle& candle, const IndicatorSnapshot& indicators, std::string rule)
        {
            rule = Upper(Trim(rule));
            const std::map<std::string, double> values = {
                { "CLOSE", candle.close },
                { "PRICE", candle.close },
                { "OPEN", candle.open },
                { "HIGH", candle.high },
                { "LOW", candle.low },
                { "RSI", indicators.rsi14 },
                { "RSI14", indicators.rsi14 },
                { "SMA20", indicators.sma20 },
                { "SMA50", indicators.sma50 },
                { "EMA12", indicators.ema12 },
                { "EMA26", indicators.ema26 },
                { "MACD", indicators.macd },
                { "ATR", indicators.atr14 },
                { "ATR14", indicators.atr14 }
            };

            std::string op;
            size_t op_pos = std::string::npos;
            for (const char* candidate : { ">=", "<=", "==", ">", "<" })
            {
                op_pos = rule.find(candidate);
                if (op_pos != std::string::npos)
                {
                    op = candidate;
                    break;
                }
            }
            if (op_pos == std::string::npos)
                return false;

            const std::string left_key = Trim(rule.substr(0, op_pos));
            const std::string right_raw = Trim(rule.substr(op_pos + op.size()));
            auto left_it = values.find(left_key);
            if (left_it == values.end())
                return false;

            double right = 0.0;
            auto right_it = values.find(right_raw);
            if (right_it != values.end())
                right = right_it->second;
            else
            {
                try
                {
                    right = std::stod(right_raw);
                }
                catch (...)
                {
                    return false;
                }
            }

            const double left = left_it->second;
            if (op == ">=")
                return left >= right;
            if (op == "<=")
                return left <= right;
            if (op == "==")
                return std::fabs(left - right) < 0.0001;
            if (op == ">")
                return left > right;
            if (op == "<")
                return left < right;
            return false;
        }

        bool EvaluateRuleExpression(const std::vector<Candle>& history, const std::string& expression)
        {
            if (history.empty())
                return false;

            const IndicatorSnapshot indicators = BuildIndicators(history, nullptr, nullptr);
            const Candle& candle = history.back();
            std::string normalized = expression;
            size_t pos = 0;
            while ((pos = normalized.find("&&", pos)) != std::string::npos)
                normalized.replace(pos, 2, " AND ");
            pos = 0;
            while ((pos = normalized.find("||", pos)) != std::string::npos)
                normalized.replace(pos, 2, " OR ");

            std::stringstream stream(normalized);
            std::string token;
            bool has_value = false;
            bool current = false;
            std::string pending = "AND";
            std::string part;
            while (std::getline(stream, token, ' '))
            {
                token = Trim(token);
                if (token.empty())
                    continue;
                const std::string upper = Upper(token);
                if (upper == "AND" || upper == "OR")
                {
                    if (!part.empty())
                    {
                        const bool value = EvaluateSingleRule(candle, indicators, part);
                        current = !has_value ? value : (pending == "AND" ? current && value : current || value);
                        has_value = true;
                        part.clear();
                    }
                    pending = upper;
                    continue;
                }
                if (!part.empty())
                    part += " ";
                part += token;
            }
            if (!part.empty())
            {
                const bool value = EvaluateSingleRule(candle, indicators, part);
                current = !has_value ? value : (pending == "AND" ? current && value : current || value);
                has_value = true;
            }
            return has_value && current;
        }

        bool RemoveTreeContents(const std::filesystem::path& directory, int& removed)
        {
            std::error_code ec;
            if (!std::filesystem::exists(directory, ec))
                return true;
            for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(directory, ec))
            {
                if (ec)
                    return false;
                if (entry.is_regular_file(ec))
                {
                    std::filesystem::remove(entry.path(), ec);
                    if (!ec)
                        ++removed;
                }
            }
            return !ec;
        }
    }

    std::string SecCompliantUserAgent(const Config& config)
    {
        std::string configured = Trim(config.sec_user_agent);
        if (configured.empty())
            configured = Trim(GetEnvUtf8(L"AEGIS_SEC_USER_AGENT"));
        if (configured.empty())
            configured = "AegisStockInvestingAI/1.0 (research-only desktop app; contact: gabriel@local.invalid)";
        configured.erase(std::remove(configured.begin(), configured.end(), '\r'), configured.end());
        configured.erase(std::remove(configured.begin(), configured.end(), '\n'), configured.end());
        return configured;
    }

    std::vector<InfoItem> BuildProviderControlRows(const Config& config)
    {
        return {
            Row("Provider interface", "Active", "C++ service facade", "Quotes, history, research, SEC, and future FRED/options/broker modules now have a shared place for policy, freshness, diagnostics, and cache controls.", "ProviderLayer"),
            Row("Alpha Vantage", BoolLabel(!Trim(config.alpha_vantage_api_key).empty()), !Trim(config.alpha_vantage_api_key).empty() ? "Live + cache" : "Demo/cache fallback", "GLOBAL_QUOTE, TIME_SERIES_DAILY, OVERVIEW, NEWS_SENTIMENT, and EARNINGS are routed with cache and diagnostics labels.", "Alpha Vantage"),
            Row("SEC EDGAR", "Configured", "Throttled/manual contact", "The HTTP user agent is explicit and configurable for SEC-friendly access: " + SecCompliantUserAgent(config), "SEC"),
            Row("Failure fallback", "Active", "Cache first", "Provider warnings and rate-limit events are logged as structured JSONL and the UI reports cache/fallback reasons.", "Diagnostics"),
            Row("Plugin registry path", "Ready", "ProviderLayer.cpp", "New providers should register capabilities, TTL, endpoint labels, and failure behavior here before UI wiring.", "Aegis")
        };
    }

    std::vector<InfoItem> BuildCachePolicyRows(const Config& config)
    {
        return {
            Row("Quote TTL", "Policy", std::to_string(std::max(15, config.alpha_quote_ttl_seconds)) + " sec", "Direct quote refresh interval and cache freshness expectations for Alpha Vantage."),
            Row("History TTL", "Policy", std::to_string(std::max(1, config.history_cache_hours)) + " hr", "Daily candle cache age before live refresh is preferred."),
            Row("Research TTL", "Policy", std::to_string(std::max(1, config.research_cache_hours)) + " hr", "Fundamentals/news/earnings/SEC bundle cache age before live refresh is preferred."),
            Row("Max cache", "Policy", std::to_string(std::max(25, config.max_cache_mb)) + " MB", "Soft limit used by prune/backup diagnostics."),
            Row("Force live", config.force_live_refresh ? "Enabled" : "Off", config.force_live_refresh ? "Bypass fresh cache" : "Use cache when fresh", "Manual research sessions can force a live refresh while still falling back safely.")
        };
    }

    std::vector<InfoItem> BuildStorageMigrationRows()
    {
        return {
            Row("Schema v1", "Created", "SQLite-ready", "Tables defined for holdings, alerts, trade_plans, notes, journal, audit_log, provider_cache, diagnostics, and schema_migrations."),
            Row("Transactions", "Planned", "Store facade", "TSV remains backward-compatible while the migration SQL and store boundary are in place for the SQLite implementation."),
            Row("Indexes", "Planned", "symbol/time", "Migration script includes symbol and timestamp indexes for portfolio, audit, diagnostics, and cache lookups."),
            Row("Append-only audit", "Active", "audit-log.jsonl", "Important UI actions now write structured audit rows as well as the legacy TSV export.")
        };
    }

    std::vector<InfoItem> BuildBrokerProfileRows()
    {
        return {
            Row("Robinhood", "Profile", "Symbol,Quantity,Average Cost", "Preview maps CSV exports into holdings/trade rows before import."),
            Row("Fidelity", "Profile", "Symbol,Quantity,Last Price,Cost Basis", "Normalization keeps quantity, cost, account, and tax-lot hints."),
            Row("Schwab", "Profile", "Symbol,Qty,Price,Market Value,Gain/Loss", "Designed for brokerage CSVs with cash and account sections."),
            Row("Webull", "Profile", "Ticker,Quantity,Avg Price", "Maps equity rows while skipping options/crypto until providers are added."),
            Row("IBKR", "Profile", "Symbol,Position,Cost Basis,Account", "Supports multi-account preview and future tax-lot tracking.")
        };
    }

    std::vector<InfoItem> BuildProductionReadinessRows(const Config& config)
    {
        return {
            Row("Research guardrails", config.paper_only_mode ? "Visible" : "Check", config.paper_only_mode ? "Paper only" : "Paper mode off", "Trade planning and exports should keep research-only language visible."),
            Row("Live trading path", config.require_manual_confirmation ? "Locked" : "Review", "Manual confirmation", "No broker module should place live orders without explicit unlock, confirmations, and audit trail."),
            Row("Secrets", "Protected", "DPAPI + scrubber", "Alpha Vantage and remembered credentials use DPAPI; diagnostics redact common key/password/cookie patterns."),
            Row("Diagnostics", "Structured", "diagnostics.jsonl", "Each provider event records timestamp, severity, endpoint, symbol, HTTP status, duration, cache state, and error."),
            Row("Tests", "Available", "--self-test", "Run scripts/test.ps1 or the executable with --self-test for parser, indicator, backtest, strategy, and diagnostics checks."),
            Row("Release", "Available", "scripts/release.ps1", "Build, smoke test, and copy clean Release artifacts into dist.")
        };
    }

    StrategyBacktestResult RunStrategyRuleBacktest(const std::vector<Candle>& candles, const std::string& expression, double fee_bps, double slippage_bps)
    {
        StrategyBacktestResult result;
        if (candles.size() < 55)
        {
            result.status = "Need at least 55 candles for a strategy test.";
            return result;
        }
        if (Trim(expression).empty())
        {
            result.status = "Enter a rule such as RSI < 35 AND CLOSE > SMA50.";
            return result;
        }

        std::vector<double> trade_returns;
        double equity = 1.0;
        double peak = 1.0;
        double max_drawdown = 0.0;
        const double fee = std::max(0.0, fee_bps) / 10000.0;
        const double slippage = std::max(0.0, slippage_bps) / 10000.0;

        for (size_t i = 50; i + 5 < candles.size(); ++i)
        {
            std::vector<Candle> history(candles.begin(), candles.begin() + static_cast<std::ptrdiff_t>(i + 1));
            if (!EvaluateRuleExpression(history, expression))
                continue;
            const double entry = candles[i + 1].open * (1.0 + slippage);
            const double exit = candles[i + 5].close * (1.0 - slippage);
            if (entry <= 0.0)
                continue;
            const double net_return = ((exit - entry) / entry) - (fee * 2.0);
            trade_returns.push_back(net_return * 100.0);
            equity *= (1.0 + net_return);
            peak = std::max(peak, equity);
            if (peak > 0.0)
                max_drawdown = std::min(max_drawdown, (equity - peak) / peak * 100.0);
        }

        BacktestResult backtest;
        backtest.trades = static_cast<int>(trade_returns.size());
        double gross_win = 0.0;
        double gross_loss = 0.0;
        for (double value : trade_returns)
        {
            if (value >= 0.0)
            {
                ++backtest.wins;
                gross_win += value;
            }
            else
            {
                ++backtest.losses;
                gross_loss += std::fabs(value);
            }
            backtest.best_trade = backtest.trades == 1 ? value : std::max(backtest.best_trade, value);
            backtest.worst_trade = backtest.trades == 1 ? value : std::min(backtest.worst_trade, value);
        }
        backtest.win_rate = backtest.trades > 0 ? (static_cast<double>(backtest.wins) / static_cast<double>(backtest.trades)) * 100.0 : 0.0;
        backtest.average_return = Average(trade_returns);
        backtest.max_drawdown = max_drawdown;
        backtest.profit_factor = gross_loss > 0.0 ? gross_win / gross_loss : (gross_win > 0.0 ? gross_win : 0.0);
        backtest.rows.push_back(Row("Rule", "Saved expression", expression, "No lookahead: signal is evaluated on closed candle data and enters next open."));
        backtest.rows.push_back(Row("Costs", "Realism", std::to_string(static_cast<int>(fee_bps)) + " bps fee / " + std::to_string(static_cast<int>(slippage_bps)) + " bps slip", "Costs are subtracted from every completed trade."));
        backtest.rows.push_back(Row("Return vol", "Risk", FormatPercent(StdDev(trade_returns)), "Standard deviation of completed trade returns."));

        result.ok = true;
        result.status = "Strategy tested on " + std::to_string(candles.size()) + " adjusted-style candle rows.";
        result.backtest = std::move(backtest);
        return result;
    }

    bool PruneProviderCaches(int older_than_days, std::string& status)
    {
        const std::filesystem::path root = AppDataDirectory();
        const auto now = std::filesystem::file_time_type::clock::now();
        const auto age = std::chrono::hours(24 * std::max(1, older_than_days));
        int removed = 0;
        std::error_code ec;
        for (const char* folder : { "history-cache", "research-cache", "sec-cache" })
        {
            const std::filesystem::path dir = root / folder;
            if (!std::filesystem::exists(dir, ec))
                continue;
            for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(dir, ec))
            {
                if (ec)
                    break;
                if (!entry.is_regular_file(ec))
                    continue;
                const auto modified = entry.last_write_time(ec);
                if (!ec && now - modified > age)
                {
                    std::filesystem::remove(entry.path(), ec);
                    if (!ec)
                        ++removed;
                }
            }
        }
        status = "Pruned " + std::to_string(removed) + " provider cache files older than " + std::to_string(std::max(1, older_than_days)) + " day(s).";
        AppendDiagnosticEvent({ "info", "cache", "prune", "", "ok", status, "", 0, 0, false });
        return true;
    }

    BackupValidationResult ValidateBackupDirectory(const std::filesystem::path& directory)
    {
        BackupValidationResult result;
        const std::vector<std::string> expected = {
            "holdings-export.csv",
            "alerts-export.csv",
            "alert-events-export.csv",
            "trade-plans-export.csv",
            "symbol-notes-export.csv",
            "trade-journal-export.csv",
            "aegis-backup-manifest.txt"
        };
        std::error_code ec;
        if (!std::filesystem::exists(directory, ec))
        {
            result.summary = "Backup folder does not exist.";
            result.issues.push_back(result.summary);
            return result;
        }
        for (const std::string& name : expected)
        {
            if (!std::filesystem::exists(directory / name, ec))
                result.issues.push_back("Missing " + name);
        }
        result.ok = result.issues.empty();
        result.summary = result.ok ? "Backup folder has the expected export files." : std::to_string(result.issues.size()) + " backup issue(s) found.";
        return result;
    }

    bool ResetDemoCaches(std::string& status)
    {
        int removed = 0;
        bool ok = true;
        ok = RemoveTreeContents(AppDataDirectory() / "history-cache", removed) && ok;
        ok = RemoveTreeContents(AppDataDirectory() / "research-cache", removed) && ok;
        ok = RemoveTreeContents(AppDataDirectory() / "sec-cache", removed) && ok;
        status = ok
            ? "Reset demo/provider caches; removed " + std::to_string(removed) + " files."
            : "Some cache files could not be reset.";
        AppendDiagnosticEvent({ ok ? "info" : "warning", "cache", "reset", "", ok ? "ok" : "partial", status, "", 0, 0, false });
        return ok;
    }
}
