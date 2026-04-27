#include "SelfTests.h"

#include "AdvancedAnalytics.h"
#include "AlertEngine.h"
#include "AppSession.h"
#include "AppStorage.h"
#include "BrokerImport.h"
#include "Csv.h"
#include "Diagnostics.h"
#include "ImportValidation.h"
#include "Json.h"
#include "PositionSizing.h"
#include "ProviderLayer.h"
#include "ReportExport.h"
#include "SettingsService.h"
#include "StockData.h"
#include "SymbolRules.h"
#include "WorkflowValidation.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace aegis
{
    namespace
    {
        struct TestRunner
        {
            int passed = 0;
            int failed = 0;
            std::ostringstream report;

            void Check(bool condition, const std::string& name, const std::string& detail = {})
            {
                if (condition)
                {
                    ++passed;
                    report << "[PASS] " << name << "\n";
                }
                else
                {
                    ++failed;
                    report << "[FAIL] " << name;
                    if (!detail.empty())
                        report << " - " << detail;
                    report << "\n";
                }
            }
        };

        StockQuote TestQuote()
        {
            StockQuote quote;
            quote.symbol = "TEST";
            quote.name = "Test Corp";
            quote.sector = "Technology";
            quote.price = 120.0;
            quote.open = 118.0;
            quote.high = 123.0;
            quote.low = 116.0;
            quote.previous_close = 117.0;
            quote.change_percent = 2.56;
            quote.volume = 2500000;
            quote.market_cap = 25000000000.0;
            quote.pe_ratio = 24.0;
            quote.beta = 1.12;
            quote.source = "self-test";
            quote.live = true;
            quote.data_quality = "Self-test provider-backed";
            return quote;
        }

        std::filesystem::path ReportPath()
        {
            return AppDataDirectory() / "self-test-report.txt";
        }
    }

    int RunSelfTests()
    {
        TestRunner runner;
        const auto near = [](double actual, double expected, double tolerance) {
            return std::fabs(actual - expected) <= tolerance;
        };

        const JsonParseResult json = ParseJson("{\"ok\":true,\"n\":42,\"rows\":[{\"symbol\":\"AAPL\"}]}");
        runner.Check(json.ok && json.value["ok"].AsBool(false) && json.value["n"].AsInt() == 42, "JSON parser handles object/bool/number");
        runner.Check(json.ok && json.value["rows"].At(0)["symbol"].AsString() == "AAPL", "JSON parser handles nested arrays");

        const std::vector<std::string> csv_cells = SplitCsvLine("AAPL,\"hello, world\",\"x\"\"y\"");
        runner.Check(csv_cells.size() == 3 && csv_cells[1] == "hello, world" && csv_cells[2] == "x\"y", "CSV parser handles quoted commas and escaped quotes");
        runner.Check(CsvCell("x\"y,z") == "\"x\"\"y,z\"", "CSV exporter escapes quotes and commas");
        const std::string csv_roundtrip_line = CsvCell("AAPL") + "," + CsvCell("10") + "," + CsvCell("note, with comma and \"quote\"");
        const std::vector<std::string> csv_roundtrip = SplitCsvLine(csv_roundtrip_line);
        runner.Check(csv_roundtrip.size() == 3 && csv_roundtrip[2] == "note, with comma and \"quote\"", "CSV import/export round-trips quoted note fields");
        const std::vector<std::string> csv_blank_cells = SplitCsvLine("AAPL,,\"\"");
        runner.Check(csv_blank_cells.size() == 3 && csv_blank_cells[1].empty() && csv_blank_cells[2].empty(), "CSV parser preserves blank cells");

        const StockQuote quote = TestQuote();
        StockQuote estimated_quote = quote;
        estimated_quote.live = false;
        estimated_quote.source = "Demo";
        estimated_quote.market_cap_estimated = true;
        estimated_quote.beta_estimated = true;
        estimated_quote.fundamentals_estimated = true;
        estimated_quote.note = "Demo fallback with estimated metrics.";
        estimated_quote.data_quality = "Demo quote / estimated metrics";
        const std::vector<InfoItem> data_quality_rows = BuildQuoteDataQualityRows(estimated_quote);
        runner.Check(DataConfidencePenalty(estimated_quote) > DataConfidencePenalty(quote), "Data confidence decays for demo or estimated quote data");
        runner.Check(!data_quality_rows.empty() &&
            data_quality_rows.front().state == "Review" &&
            std::any_of(data_quality_rows.begin(), data_quality_rows.end(), [](const InfoItem& row) {
                return row.name == "Fundamentals" && row.label == "Estimated";
            }), "Data quality rows label estimated decision inputs");

        std::vector<Candle> candles = BuildSyntheticCandles(quote, 120);
        runner.Check(candles.size() == 120 && candles.back().close > 0.0, "Synthetic candle builder returns usable rows");

        std::vector<Candle> aggregation_fixture;
        for (int i = 0; i < 10; ++i)
        {
            Candle candle;
            candle.date = "2024-01-" + std::string(i + 1 < 10 ? "0" : "") + std::to_string(i + 1);
            candle.open = static_cast<double>(i + 1);
            candle.high = static_cast<double>(i + 2);
            candle.low = static_cast<double>(i);
            candle.close = static_cast<double>(i) + 1.5;
            candle.volume = 100 + i;
            aggregation_fixture.push_back(candle);
        }
        const std::vector<Candle> weekly_candles = AggregateCandles(aggregation_fixture, CandleAggregation::Weekly);
        const std::vector<Candle> monthly_candles = AggregateCandles(aggregation_fixture, CandleAggregation::Monthly);
        runner.Check(weekly_candles.size() == 2 && near(weekly_candles.front().open, 1.0, 0.001) &&
            near(weekly_candles.front().close, 7.5, 0.001) && weekly_candles.front().volume == 721, "Weekly candle aggregation preserves OHLCV");
        runner.Check(monthly_candles.size() == 1 && near(monthly_candles.front().high, 11.0, 0.001) &&
            near(monthly_candles.front().low, 0.0, 0.001) && monthly_candles.front().volume == 1045, "Monthly candle aggregation preserves high low and volume");

        const IndicatorSnapshot indicators = BuildIndicators(candles, nullptr, nullptr);
        runner.Check(indicators.sma20 > 0.0 && indicators.sma50 > 0.0, "Indicators calculate moving averages");
        runner.Check(indicators.rsi14 >= 0.0 && indicators.rsi14 <= 100.0, "Indicators calculate RSI range");
        std::vector<Candle> indicator_fixture;
        for (int i = 1; i <= 60; ++i)
        {
            Candle candle;
            candle.date = "fixture-" + std::to_string(i);
            candle.open = static_cast<double>(i) - 0.25;
            candle.high = static_cast<double>(i) + 1.0;
            candle.low = static_cast<double>(i) - 1.0;
            candle.close = static_cast<double>(i);
            candle.volume = 1000000 + i;
            indicator_fixture.push_back(candle);
        }
        const IndicatorSnapshot fixture_indicators = BuildIndicators(indicator_fixture, nullptr, nullptr);
        runner.Check(near(fixture_indicators.sma20, 50.5, 0.001) && near(fixture_indicators.sma50, 35.5, 0.001), "Indicators calculate SMA fixtures");
        runner.Check(fixture_indicators.ema12 > fixture_indicators.ema26 && fixture_indicators.macd > 0.0, "Indicators calculate EMA and MACD trend");
        runner.Check(near(fixture_indicators.rsi14, 99.0099, 0.01), "Indicators calculate RSI for one-way gains");
        runner.Check(near(fixture_indicators.bollinger_mid, 50.5, 0.001) &&
            near(fixture_indicators.bollinger_upper, 62.0325, 0.01) &&
            near(fixture_indicators.bollinger_lower, 38.9674, 0.01), "Indicators calculate Bollinger Bands");
        runner.Check(near(fixture_indicators.atr14, 2.0, 0.001), "Indicators calculate ATR");
        std::vector<Candle> drawdown_fixture = indicator_fixture;
        drawdown_fixture.back().high = 49.0;
        drawdown_fixture.back().low = 47.0;
        drawdown_fixture.back().close = 48.0;
        const IndicatorSnapshot drawdown_indicators = BuildIndicators(drawdown_fixture, nullptr, nullptr);
        runner.Check(drawdown_indicators.drawdown < -18.0 && drawdown_indicators.drawdown > -19.0, "Indicators calculate drawdown");
        runner.Check(std::isfinite(fixture_indicators.volatility) && fixture_indicators.volatility > 0.0, "Indicators calculate annualized volatility");

        const BacktestResult backtest = RunSignalBacktest(candles, "Momentum");
        runner.Check(backtest.trades >= 0 && backtest.win_rate >= 0.0, "Built-in backtest returns sane stats");
        std::vector<Candle> no_lookahead_fixture;
        for (int i = 0; i < 57; ++i)
        {
            Candle candle;
            candle.date = "lookahead-" + std::to_string(i);
            candle.open = 100.0 + static_cast<double>(i);
            candle.close = 100.0 + static_cast<double>(i);
            candle.high = candle.close + 1.0;
            candle.low = candle.close - 1.0;
            candle.volume = 1000000;
            no_lookahead_fixture.push_back(candle);
        }
        const BacktestResult baseline_lookahead = RunSignalBacktest(no_lookahead_fixture, "Momentum");
        std::vector<Candle> future_shocked = no_lookahead_fixture;
        for (size_t i = 51; i < future_shocked.size(); ++i)
        {
            future_shocked[i].open = 25.0;
            future_shocked[i].close = 25.0;
            future_shocked[i].high = 26.0;
            future_shocked[i].low = 24.0;
        }
        const BacktestResult shocked_lookahead = RunSignalBacktest(future_shocked, "Momentum");
        runner.Check(baseline_lookahead.trades == 1 && shocked_lookahead.trades == 1 &&
            baseline_lookahead.average_return > 0.0 && shocked_lookahead.average_return < 0.0,
            "Backtest entries do not depend on future exit candles");

        PositionSizingInput sizing_input;
        sizing_input.portfolio_cash = 100000.0;
        sizing_input.max_position_amount = 50000.0;
        sizing_input.max_portfolio_risk_percent = 1.0;
        sizing_input.entry = 100.0;
        sizing_input.stop = 95.0;
        sizing_input.target = 115.0;
        sizing_input.beta = 1.5;
        sizing_input.change_percent = 3.0;
        sizing_input.score = 70;
        sizing_input.atr14 = 4.0;
        sizing_input.mode = PositionSizingMode::FixedDollar;
        PositionSizingResult sizing = CalculatePositionSize(sizing_input);
        runner.Check(sizing.ok && sizing.shares == 500 && near(sizing.notional, 50000.0, 0.001), "Position sizing fixed dollar uses max position cap");
        sizing_input.mode = PositionSizingMode::FixedRisk;
        sizing = CalculatePositionSize(sizing_input);
        runner.Check(sizing.ok && sizing.shares == 200 && near(sizing.planned_risk, 1000.0, 0.001), "Position sizing fixed risk uses risk budget");
        sizing_input.mode = PositionSizingMode::VolatilityAdjusted;
        sizing_input.max_portfolio_risk_percent = 5.0;
        sizing = CalculatePositionSize(sizing_input);
        runner.Check(sizing.ok && sizing.shares == 250 && near(sizing.notional, 25000.0, 0.001), "Position sizing volatility-adjusted reduces allocation");
        sizing_input.mode = PositionSizingMode::AtrStop;
        sizing_input.max_portfolio_risk_percent = 1.0;
        sizing = CalculatePositionSize(sizing_input);
        runner.Check(sizing.ok && sizing.shares == 166 && near(sizing.effective_stop, 94.0, 0.001), "Position sizing ATR stop uses ATR-adjusted stop");
        sizing_input.mode = PositionSizingMode::KellyLite;
        sizing = CalculatePositionSize(sizing_input);
        runner.Check(sizing.ok && sizing.shares == 60 && near(sizing.notional, 6000.0, 0.001), "Position sizing Kelly-lite caps allocation conservatively");

        const StrategyBacktestResult strategy = RunStrategyRuleBacktest(candles, "RSI < 80 AND CLOSE > SMA50", 1.0, 2.0);
        runner.Check(strategy.ok && strategy.backtest.trades >= 0, "Strategy builder parses AND rules");

        ProviderRequest provider_request;
        provider_request.provider = "Alpha Vantage";
        provider_request.endpoint = "TIME_SERIES_DAILY";
        provider_request.symbol = "aapl";
        provider_request.cache_exists = true;
        provider_request.cache_fresh = true;
        ProviderResponseMetadata provider_metadata = EvaluateProviderRequest(provider_request);
        runner.Check(provider_metadata.use_cache && !provider_metadata.should_fetch_live && provider_metadata.cache_age_label == "fresh cache hit", "Provider policy uses fresh cache hits");
        provider_request.cache_exists = false;
        provider_request.cache_fresh = false;
        provider_metadata = EvaluateProviderRequest(provider_request);
        runner.Check(provider_metadata.should_fetch_live && provider_metadata.cache_age_label == "cache miss", "Provider policy detects cache misses");
        provider_request.cache_exists = true;
        provider_request.cache_fresh = false;
        provider_metadata = EvaluateProviderRequest(provider_request);
        runner.Check(provider_metadata.should_fetch_live && provider_metadata.stale && provider_metadata.cache_age_label == "stale cache available", "Provider policy detects stale cache before live refresh");
        provider_request.live_attempted = true;
        provider_request.live_success = true;
        provider_metadata = EvaluateProviderRequest(provider_request);
        runner.Check(provider_metadata.live && provider_metadata.cache_age_label == "live fetch", "Provider policy reports live fetch success");
        ProviderRequest limited_request = provider_request;
        limited_request.live_success = false;
        limited_request.rate_limited = true;
        limited_request.http_status = 429;
        provider_metadata = EvaluateProviderRequest(limited_request);
        runner.Check(provider_metadata.rate_limited && provider_metadata.fallback && provider_metadata.use_cache && provider_metadata.cache_age_label == "provider-limit cache fallback", "Provider policy falls back to cache on rate limits");
        runner.Check(IsProviderRateLimitResponse(200, "Thank you for using Alpha Vantage. Our standard API call frequency is limited."), "Provider policy detects body-based rate-limit notices");
        ProviderRequest retry_request;
        retry_request.provider = "SEC";
        retry_request.endpoint = "submissions";
        retry_request.symbol = "MSFT";
        retry_request.live_attempted = true;
        retry_request.error = "timeout";
        retry_request.attempt = 1;
        retry_request.max_attempts = 3;
        retry_request.retry_backoff_ms = 500;
        provider_metadata = EvaluateProviderRequest(retry_request);
        runner.Check(provider_metadata.retry && provider_metadata.next_retry_ms == 500 && !provider_metadata.fallback, "Provider policy schedules retry before fallback");
        retry_request.cache_exists = true;
        retry_request.cache_fresh = false;
        retry_request.attempt = 3;
        provider_metadata = EvaluateProviderRequest(retry_request);
        runner.Check(provider_metadata.fallback && provider_metadata.use_cache && provider_metadata.stale && provider_metadata.fallback_reason == "timeout", "Provider policy uses stale cache after retries are exhausted");
        runner.Check(SecThrottleDelayMs(0, 120) == 120 && SecThrottleDelayMs(200, 120) == 0, "Provider policy calculates SEC throttle delays");
        Config provider_config;
        provider_config.alpha_vantage_api_key = "demo";
        provider_config.force_live_refresh = true;
        const std::vector<InfoItem> capability_rows = BuildProviderCapabilityRows(provider_config);
        runner.Check(capability_rows.size() >= 8 && capability_rows.front().name == "Quotes", "Provider capability registry exposes supported data domains");
        const std::vector<InfoItem> cache_policy_rows = BuildCachePolicyRows(provider_config);
        runner.Check(std::any_of(cache_policy_rows.begin(), cache_policy_rows.end(), [](const InfoItem& row) {
            return row.name == "Force-live risk" && row.state == "Review";
        }), "Cache policy reports force-live rate-limit risk");
        AppendDiagnosticEvent({ "warning", "Alpha Vantage", "GLOBAL_QUOTE", "AAPL", "rate-limit", "standard API call frequency limited", "", 200, 12, true });
        const std::vector<InfoItem> provider_diagnostics = BuildProviderDiagnosticRows(1);
        runner.Check(!provider_diagnostics.empty() && provider_diagnostics.front().name.find("Alpha Vantage") != std::string::npos, "Provider diagnostics table parses recent structured events");
        const std::vector<InfoItem> provider_limits = BuildProviderLimitRows(8);
        runner.Check(!provider_limits.empty() && provider_limits.front().state == "rate-limit", "Provider rate-limit diagnostics surface recent limit events");

        const std::string sensitive_text =
            "https://example.test/query?apikey=SECRET123&api_key=SECRET456&access_token=ACCESS789&symbol=AAPL "
            "password=hunter2 Authorization: Bearer AUTH_TOKEN_123\nCookie: session=COOKIE_SECRET_123\n"
            "{\"password\":\"JSON_PASS_123\",\"alpha_vantage_api_key\":\"ALPHA_JSON_123\",\"token\":\"JSON_TOKEN_123\"}";
        const std::vector<std::string> sensitive_tokens = {
            "SECRET123",
            "SECRET456",
            "ACCESS789",
            "hunter2",
            "AUTH_TOKEN_123",
            "COOKIE_SECRET_123",
            "JSON_PASS_123",
            "ALPHA_JSON_123",
            "JSON_TOKEN_123",
            "ERR_TOKEN_123",
            "ERR_PASS_123"
        };
        const auto has_no_sensitive_tokens = [&](const std::string& text) {
            for (const std::string& token : sensitive_tokens)
            {
                if (text.find(token) != std::string::npos)
                    return false;
            }
            return true;
        };
        const std::string redacted = RedactSecrets(sensitive_text);
        runner.Check(has_no_sensitive_tokens(redacted), "Diagnostics redact API keys, cookies, auth headers, bearer tokens, passwords, and URL query keys");
        AppendDiagnosticEvent({ "warn", "self-test", "redaction", "TEST", "scrub", sensitive_text, "authorization=ERR_TOKEN_123&password=ERR_PASS_123", 401, 4, false });
        std::ostringstream recent_diagnostics;
        for (const std::string& line : LoadRecentDiagnosticLines(5))
            recent_diagnostics << line << '\n';
        runner.Check(has_no_sensitive_tokens(recent_diagnostics.str()), "Diagnostics writer redacts structured fields before persistence");
        std::string diagnostics_export_status;
        const std::filesystem::path diagnostics_export_dir = AppDataDirectory() / "self-test-diagnostics-export";
        runner.Check(ExportDiagnosticsSnapshot(diagnostics_export_dir, diagnostics_export_status), "Diagnostics export creates snapshot");
        std::ifstream exported_diagnostics(diagnostics_export_dir / "diagnostics.jsonl");
        std::ostringstream exported_diagnostics_text;
        exported_diagnostics_text << exported_diagnostics.rdbuf();
        runner.Check(has_no_sensitive_tokens(exported_diagnostics_text.str()), "Diagnostics export does not expose redacted self-test secrets");

        PriceAlert alert;
        alert.symbol = "TEST";
        alert.trigger_price = 100.0;
        alert.above = true;
        alert.enabled = true;
        runner.Check(IsPriceAlertTriggered(alert, quote), "Alert engine triggers above-price alerts");
        PriceAlert below_alert = alert;
        below_alert.above = false;
        below_alert.trigger_price = 130.0;
        runner.Check(IsPriceAlertTriggered(below_alert, quote), "Alert engine triggers below-price alerts");
        below_alert.enabled = false;
        runner.Check(!IsPriceAlertTriggered(below_alert, quote), "Alert engine ignores disabled alerts");
        const AlertEvaluationResult first_alert_check = EvaluatePriceAlerts({ alert }, { quote }, {}, "Self-test quote board");
        runner.Check(first_alert_check.checked == 1 && first_alert_check.triggered == 1 && first_alert_check.new_events.size() == 1 && first_alert_check.rows.size() == 1,
            "Alert engine evaluates monitored alert checks and creates new events");
        std::vector<AlertEvent> events;
        events.push_back(MakeAlertEvent(alert, quote));
        const AlertEvaluationResult suppressed_alert_check = EvaluatePriceAlerts({ alert }, { quote }, events, "Self-test quote board");
        runner.Check(suppressed_alert_check.suppressed == 1 && suppressed_alert_check.new_events.empty(), "Alert engine suppresses duplicate events during scheduled checks");
        runner.Check(CountUnreadAlertEvents(events) == 1, "Alert engine counts unread alert events");
        StockQuote later_quote = quote;
        later_quote.price = 132.0;
        const AlertOutcome alert_outcome = AlertOutcomeForEvent(events.front(), { later_quote });
        runner.Check(alert_outcome.has_quote && alert_outcome.label == "Extended" && near(alert_outcome.change_percent, 10.0, 0.001), "Alert engine tracks current outcome after trigger");
        const std::vector<InfoItem> notification_rows = BuildAlertNotificationRows(events, { later_quote }, 5);
        runner.Check(!notification_rows.empty() && notification_rows.front().name == "TEST" && notification_rows.front().value.find("Extended") != std::string::npos, "Alert engine builds notification center rows");
        runner.Check(IsAlertEventSuppressed(alert, events), "Alert engine suppresses duplicate open events");
        AcknowledgeAlertEvent(events.front());
        runner.Check(AlertEventState(events.front()) == "Acknowledged", "Alert engine acknowledges events");
        runner.Check(CountUnreadAlertEvents(events) == 0, "Alert engine removes acknowledged events from unread count");
        runner.Check(!IsAlertEventSuppressed(alert, events), "Alert engine allows a new event after acknowledgement");
        SnoozeAlertEvent(events.front(), 5);
        runner.Check(AlertEventState(events.front()) == "Snoozed", "Alert engine snoozes events");
        runner.Check(IsAlertEventSuppressed(alert, events) && !AlertEventSnoozeLabel(events.front()).empty(), "Alert engine suppresses snoozed events");
        const std::filesystem::path alert_history_path = AppDataDirectory() / "self-test-alert-events.tsv";
        runner.Check(SaveAlertEvents(events, alert_history_path), "Alert engine persists trigger history");
        const std::vector<AlertEvent> loaded_alert_events = LoadAlertEvents(alert_history_path);
        runner.Check(loaded_alert_events.size() == 1 &&
            loaded_alert_events.front().symbol == "TEST" &&
            loaded_alert_events.front().alert_key == AlertKey(alert) &&
            loaded_alert_events.front().snoozed_until_epoch == events.front().snoozed_until_epoch,
            "Alert engine reloads trigger history");
        std::error_code alert_history_ec;
        std::filesystem::remove(alert_history_path, alert_history_ec);

        const std::filesystem::path broker_dir = AppDataDirectory() / "self-test-broker-import";
        std::error_code broker_ec;
        std::filesystem::remove_all(broker_dir, broker_ec);
        std::filesystem::create_directories(broker_dir, broker_ec);
        {
            std::ofstream broker_file(broker_dir / "robinhood-import.csv", std::ios::trunc);
            broker_file << "Symbol,Quantity,Average Cost,Type\n";
            broker_file << "AAPL,2,150.25,Stock\n";
            broker_file << "BAD,0,10,Stock\n";
            broker_file << "BTC,1,100,Crypto\n";
        }
        {
            std::ofstream broker_file(broker_dir / "fidelity-import.csv", std::ios::trunc);
            broker_file << "Ticker,Current Quantity,Cost Basis Total,Account Name,Security Type\n";
            broker_file << "MSFT,4,1000.00,Taxable,Stock\n";
        }
        {
            std::ofstream broker_file(broker_dir / "schwab-import.csv", std::ios::trunc);
            broker_file << "Symbol,Quantity,Cost Basis,Account Number,Type\n";
            broker_file << "NVDA,3,750.00,Schwab One,Equity\n";
        }
        {
            std::ofstream broker_file(broker_dir / "webull-import.csv", std::ios::trunc);
            broker_file << "Ticker,Position,Avg Price,Asset Type\n";
            broker_file << "AMD,5,125.50,Stock\n";
        }
        {
            std::ofstream broker_file(broker_dir / "ibkr-import.csv", std::ios::trunc);
            broker_file << "Underlying Symbol,Position,Cost Basis per Share,Account,Instrument Type\n";
            broker_file << "TSLA,6,210.00,Paper,Stock\n";
        }
        {
            std::ofstream broker_file(broker_dir / "broker-import.csv", std::ios::trunc);
            broker_file << "Instrument,Shares,Book Cost,Portfolio,Type\n";
            broker_file << "BRK/B,1,402.00,Generic,Stock\n";
        }
        const std::vector<BrokerImportRow> broker_rows = PreviewBrokerImport(broker_dir);
        const auto has_valid_profile = [&](const std::string& profile) {
            for (const BrokerImportRow& row : broker_rows)
            {
                if (row.valid && row.profile == profile)
                    return true;
            }
            return false;
        };
        bool fidelity_total_cost_mapped = false;
        bool generic_symbol_alias_mapped = false;
        for (const BrokerImportRow& row : broker_rows)
        {
            if (row.valid && row.profile == "Fidelity" && row.symbol == "MSFT" && std::fabs(row.average_cost - 250.0) < 0.001)
                fidelity_total_cost_mapped = true;
            if (row.valid && row.profile == "Generic Broker" && row.symbol == "BRK.B")
                generic_symbol_alias_mapped = true;
        }
        runner.Check(CountValidBrokerRows(broker_rows) == 7, "Broker import accepts valid equity rows across profiles");
        runner.Check(broker_rows.size() == 9, "Broker import keeps preview/error rows");
        runner.Check(has_valid_profile("Robinhood") && has_valid_profile("Fidelity") && has_valid_profile("Schwab") &&
            has_valid_profile("Webull") && has_valid_profile("IBKR") && has_valid_profile("Generic Broker"),
            "Broker import profiles cover Robinhood, Fidelity, Schwab, Webull, IBKR, and generic CSVs");
        runner.Check(fidelity_total_cost_mapped && generic_symbol_alias_mapped, "Broker import maps total cost and special ticker symbols");

        StockSignal signal;
        signal.symbol = quote.symbol;
        signal.company = quote.name;
        signal.rating = "Watch";
        signal.posture = "Test posture";
        signal.score = 72;
        signal.confidence = 68;
        signal.target_price = 135.0;
        signal.stop_level = 112.0;
        signal.risk = "Defined";
        signal.upside = "+12%";
        signal.thesis = "Self-test report thesis.";
        const std::vector<InfoItem> model_explanation = BuildModelExplanation(quote, signal, indicators);
        const std::vector<InfoItem> trade_checklist = BuildTradeChecklist(quote, signal, indicators);
        const std::vector<InfoItem> risk_critic = BuildRiskCritic(quote, signal, indicators);
        const auto has_row = [](const std::vector<InfoItem>& rows, const std::string& name) {
            for (const InfoItem& row : rows)
            {
                if (row.name == name)
                    return true;
            }
            return false;
        };
        runner.Check(model_explanation.size() == 5 &&
            has_row(model_explanation, "Price momentum") &&
            has_row(model_explanation, "Trend stack") &&
            has_row(model_explanation, "RSI") &&
            has_row(model_explanation, "MACD") &&
            has_row(model_explanation, "Risk"),
            "Model explanation exposes traceable score factors");
        runner.Check(!model_explanation.front().weight.empty() && model_explanation.front().detail.find("conviction") != std::string::npos,
            "Model explanation includes score weight and rationale");
        runner.Check(trade_checklist.size() == 6 && has_row(trade_checklist, "Risk/reward") && has_row(trade_checklist, "Market regime"),
            "Trade checklist includes risk/reward and regime checks");
        runner.Check(risk_critic.size() == 5 && has_row(risk_critic, "Invalidation") && has_row(risk_critic, "News risk"),
            "Risk critic includes invalidation and news-risk rows");
        const std::string sec_fixture = R"JSON({
            "filings": {
                "recent": {
                    "form": ["10-K", "S-1", "4", "8-K"],
                    "filingDate": ["2026-02-01", "2026-01-20", "2026-01-15", "2026-01-10"],
                    "accessionNumber": ["0000320193-26-000001", "0000320193-26-000002", "0000320193-26-000003", "0000320193-26-000004"],
                    "primaryDocument": ["aapl-20260930.htm", "aapl-s1.htm", "xslF345X05/form4.xml", "aapl-8k.htm"]
                }
            }
        })JSON";
        const std::vector<FilingItem> sec_filings = ParseSecSubmissionsFilings(sec_fixture, "aapl", "0000320193");
        runner.Check(sec_filings.size() == 3 && sec_filings.front().form == "10-K" && sec_filings.front().title == "AAPL 10-K filing",
            "SEC parser keeps decision-relevant filing forms");
        runner.Check(sec_filings.front().url.find("/320193/000032019326000001/aapl-20260930.htm") != std::string::npos &&
            sec_filings.front().risk_change.find("prior annual") != std::string::npos,
            "SEC parser builds archive links and risk review hints");
        runner.Check(ParseSecSubmissionsFilings("{bad json", "AAPL", "0000320193").empty(), "SEC parser rejects malformed JSON");
        const std::string risk_diff = BuildRiskFactorChangeSummary(
            "Our business faces competition, supplier concentration, and cybersecurity incidents.",
            "Our business faces competition, supplier concentration, cybersecurity incidents, liquidity stress, litigation, and regulation.");
        runner.Check(risk_diff.find("changed") != std::string::npos &&
            risk_diff.find("liquidity") != std::string::npos &&
            risk_diff.find("litigation") != std::string::npos,
            "SEC risk-factor diff highlights added risks");
        SymbolResearchReport symbol_report;
        symbol_report.quote = quote;
        symbol_report.signal = signal;
        symbol_report.indicators = indicators;
        symbol_report.backtest = backtest;
        symbol_report.fundamentals = BuildFundamentals(quote);
        symbol_report.explanation = model_explanation;
        symbol_report.checklist = trade_checklist;
        symbol_report.risk_critic = risk_critic;
        std::filesystem::path symbol_report_path;
        std::string report_status;
        runner.Check(ExportSymbolResearchReport(symbol_report, symbol_report_path, report_status) && std::filesystem::exists(symbol_report_path), "Symbol HTML report exports");

        StockState briefing_state = MakeDemoStockState();
        DailyBriefingReport briefing;
        briefing.state = briefing_state;
        briefing.focus = BuildFocusItems(briefing_state, {});
        std::filesystem::path briefing_path;
        runner.Check(ExportDailyBriefingReport(briefing, briefing_path, report_status) && std::filesystem::exists(briefing_path), "Daily briefing HTML exports");

        Config config;
        config.sec_user_agent = "AegisStockInvestingAI/1.0 contact@example.com";
        runner.Check(SecCompliantUserAgent(config).find("AegisStockInvestingAI") != std::string::npos, "SEC user agent is configurable");

        const std::filesystem::path session_path = AppDataDirectory() / "self-test-session.tsv";
        AppSessionState saved_session;
        saved_session.selected_tab = 99;
        saved_session.selected_symbol = "msft";
        saved_session.compare_symbols = "aapl,msft,aapl";
        saved_session.chart_days = 5000;
        saved_session.chart_aggregation = 99;
        saved_session.show_chart_volume = false;
        saved_session.show_trend_indicators = true;
        saved_session.show_momentum_indicators = false;
        saved_session.show_volatility_indicators = true;
        saved_session.show_relative_indicators = false;
        saved_session.strategy_rule = "RSI < 35 AND CLOSE > SMA50";
        runner.Check(SaveAppSessionState(saved_session, session_path), "Session store writes TSV state");
        const AppSessionState loaded_session = LoadAppSessionState(session_path);
        runner.Check(loaded_session.loaded && loaded_session.selected_tab == 9 && loaded_session.selected_symbol == "MSFT", "Session store normalizes tab and symbol");
        runner.Check(loaded_session.compare_symbols == "AAPL,MSFT" && loaded_session.chart_days == 1260 && loaded_session.chart_aggregation == 2, "Session store normalizes basket and chart layout");
        runner.Check(!loaded_session.show_chart_volume && loaded_session.show_trend_indicators && !loaded_session.show_momentum_indicators && loaded_session.show_volatility_indicators && !loaded_session.show_relative_indicators, "Session store persists chart layout toggles");

        PersistentAppData storage_rows;
        storage_rows.holdings.push_back({ "AAPL", 1.0, 100.0, "Test holding" });
        storage_rows.price_alerts.push_back(alert);
        storage_rows.alert_events = events;
        storage_rows.trade_plans.push_back({ "now", "AAPL", "Watch", 100.0, 95.0, 110.0, 1, 5.0, 10.0, "Test thesis", "Open" });
        storage_rows.symbol_notes.push_back({ "AAPL", "test", "note", "now" });
        storage_rows.journal_entries.push_back({ "now", "AAPL", "Entry", "reason", "exit", "tag", "mistake", "A", 12.0 });
        const std::vector<InfoItem> storage_summary = BuildPersistentDataRows(storage_rows);
        runner.Check(storage_summary.size() == 6 && storage_summary.front().value == "1", "Storage facade summarizes persistent app data");
        const std::filesystem::path storage_dir = AppDataDirectory() / "self-test-storage";
        std::error_code storage_ec;
        std::filesystem::remove_all(storage_dir, storage_ec);
        std::filesystem::create_directories(storage_dir, storage_ec);
        const std::filesystem::path holdings_path = storage_dir / "holdings.tsv";
        const std::filesystem::path alerts_path = storage_dir / "alerts.tsv";
        const std::filesystem::path plans_path = storage_dir / "plans.tsv";
        runner.Check(SavePortfolioHoldings(storage_rows.holdings, holdings_path), "Storage writes holdings TSV");
        const std::vector<PortfolioHolding> loaded_holdings = LoadPortfolioHoldings(holdings_path);
        runner.Check(loaded_holdings.size() == 1 && loaded_holdings.front().symbol == "AAPL" && loaded_holdings.front().note == "Test holding", "Storage reloads holdings TSV");
        runner.Check(SavePriceAlerts(storage_rows.price_alerts, alerts_path), "Storage writes alerts TSV");
        const std::vector<PriceAlert> loaded_alerts = LoadPriceAlerts(alerts_path);
        runner.Check(loaded_alerts.size() == 1 && loaded_alerts.front().symbol == "TEST" && loaded_alerts.front().trigger_price == 100.0, "Storage reloads alerts TSV");
        runner.Check(SaveTradePlans(storage_rows.trade_plans, plans_path), "Storage writes trade plans TSV");
        const std::vector<TradePlan> loaded_plans = LoadTradePlans(plans_path);
        runner.Check(loaded_plans.size() == 1 && loaded_plans.front().symbol == "AAPL" && loaded_plans.front().shares == 1, "Storage reloads trade plans TSV");
        runner.Check(!SavePortfolioHoldings(storage_rows.holdings, storage_dir), "Storage reports write failure for invalid file path");
        const BackupValidationResult missing_backup = ValidateBackupDirectory(storage_dir / "missing-backup");
        runner.Check(!missing_backup.ok && !missing_backup.issues.empty(), "Backup validation reports missing folder");
        const std::filesystem::path backup_dir = storage_dir / "backup";
        std::filesystem::create_directories(backup_dir, storage_ec);
        for (const char* name : { "holdings-export.csv", "alerts-export.csv", "alert-events-export.csv", "trade-plans-export.csv", "symbol-notes-export.csv", "trade-journal-export.csv", "aegis-backup-manifest.txt" })
        {
            std::ofstream file(backup_dir / name, std::ios::trunc);
            file << "self-test\n";
        }
        const BackupValidationResult valid_backup = ValidateBackupDirectory(backup_dir);
        runner.Check(valid_backup.ok, "Backup validation accepts complete backup folder");
        std::filesystem::remove_all(storage_dir, storage_ec);

        Config settings_config;
        settings_config.refresh_seconds = 1;
        settings_config.max_symbols = 999;
        settings_config.alpha_quote_ttl_seconds = 1;
        SettingsInput settings_input;
        settings_input.watchlist = "aapl, msft, aapl";
        settings_input.auth_base_url = "";
        settings_input.sec_user_agent = "";
        settings_input.ui_compact_mode = true;
        settings_input.ui_high_contrast = true;
        settings_input.font_scale_percent = 200;
        settings_config = ApplySettingsInput(settings_config, settings_input);
        runner.Check(settings_config.watchlist == "AAPL,MSFT" && settings_config.auth_base_url == "http://127.0.0.1:8000", "Settings service normalizes watchlist and auth URL");
        runner.Check(settings_config.refresh_seconds == 30 && settings_config.max_symbols == 50 && settings_config.alpha_quote_ttl_seconds == 15, "Settings service clamps provider settings");
        runner.Check(settings_config.ui_compact_mode && settings_config.ui_high_contrast && settings_config.font_scale_percent == 150, "Settings service persists and clamps UI preferences");
        runner.Check(BuildSettingsHealthRows(settings_config).size() == 8, "Settings service builds health rows");
        settings_config.watchlist = "AAPL,MSFT,NVDA";
        settings_config.max_symbols = 2;
        const WatchlistLimitResult limits = ValidateWatchlistLimits(settings_config);
        runner.Check(!limits.within_limit && limits.symbol_count == 3 && limits.refresh_cap == 2, "Settings service flags watchlists above refresh cap");

        const SymbolValidationResult brk = ValidateTickerSymbol("$brk/b");
        const SymbolValidationResult bad_symbol = ValidateTickerSymbol("A@PL");
        runner.Check(brk.ok && brk.symbol == "BRK.B", "Symbol rules normalize class-share tickers");
        runner.Check(!bad_symbol.ok && bad_symbol.reason.find("unsupported") != std::string::npos, "Symbol rules reject unsupported characters");
        runner.Check(JoinWatchlist(SplitWatchlist("aapl, $msft, brk/b, A@PL, aapl")) == "AAPL,MSFT,BRK.B", "Watchlist splitter validates and deduplicates symbols");

        const HoldingImportResult holding_import = ParseHoldingImportRow({ "$msft", "3.5", "225.10", "Long term" });
        const HoldingImportResult bad_holding_import = ParseHoldingImportRow({ "A@PL", "0", "bad" });
        const HoldingImportResult blank_holding_import = ParseHoldingImportRow(SplitCsvLine(""));
        const HoldingImportResult quoted_holding_import = ParseHoldingImportRow(SplitCsvLine("AAPL,2,150.50,\"Core, long-term\""));
        const AlertImportResult alert_import = ParseAlertImportRow({ "brk/b", "400", "below", "yes", "Watch level" });
        const AlertImportResult malformed_alert_import = ParseAlertImportRow(SplitCsvLine("MSFT,not-a-price,above,true"));
        const TradePlanImportResult plan_import = ParseTradePlanImportRow({ "", "nvda", "Watch", "100", "90", "120", "2", "20", "40", "", "Test" });
        const JournalImportResult bad_journal_import = ParseJournalImportRow({ "now", "AAPL", "", "reason", "", "tag", "", "A", "oops" });
        runner.Check(holding_import.ok && holding_import.row.symbol == "MSFT" && holding_import.row.shares == 3.5, "Import validation accepts valid holding rows");
        runner.Check(!bad_holding_import.ok, "Import validation rejects malformed holding rows");
        runner.Check(!blank_holding_import.ok && quoted_holding_import.ok && quoted_holding_import.row.note == "Core, long-term", "Import validation handles blank and quoted holding rows");
        runner.Check(alert_import.ok && alert_import.row.symbol == "BRK.B" && !alert_import.row.above, "Import validation normalizes alert rows");
        runner.Check(!malformed_alert_import.ok, "Import validation rejects malformed alert rows");
        runner.Check(plan_import.ok && plan_import.row.symbol == "NVDA" && plan_import.row.status == "Open", "Import validation accepts trade plan rows");
        runner.Check(!bad_journal_import.ok, "Import validation rejects journal rows with missing action");

        const HoldingDraftResult holding_draft = ValidateHoldingDraft("aapl", 4.0, 125.0, "Core");
        const HoldingDraftResult bad_holding_draft = ValidateHoldingDraft("aapl", -1.0, 125.0, "Core");
        std::vector<PortfolioHolding> duplicate_holdings = { { "AAPL", 1.0, 100.0, "" }, { "MSFT", 2.0, 200.0, "" } };
        PriceAlert duplicate_alert_a{ "AAPL", 150.0, true, true, "" };
        PriceAlert duplicate_alert_b{ "MSFT", 250.0, false, true, "" };
        std::vector<PriceAlert> duplicate_alerts = { duplicate_alert_a, duplicate_alert_b };
        const AlertDraftResult alert_draft = ValidateAlertDraft("aapl", 150.0, true, true, "Breakout");
        runner.Check(holding_draft.ok && holding_draft.row.symbol == "AAPL", "Workflow validation accepts holding drafts");
        runner.Check(!bad_holding_draft.ok, "Workflow validation rejects bad holding drafts");
        runner.Check(FindDuplicateHoldingIndex(duplicate_holdings, "aapl", -1) == 0, "Workflow validation finds duplicate holdings");
        runner.Check(alert_draft.ok && FindDuplicateAlertIndex(duplicate_alerts, alert_draft.row, -1) == 0, "Workflow validation finds duplicate alerts");
        TradePlan draft_plan{ "", "aapl", "Watch", 100.0, 95.0, 115.0, 3, 15.0, 45.0, "Plan thesis", "Open" };
        TradePlan bad_draft_plan = draft_plan;
        bad_draft_plan.stop = 101.0;
        const TradePlanDraftResult trade_plan_draft = ValidateTradePlanDraft(draft_plan);
        const TradePlanDraftResult bad_trade_plan_draft = ValidateTradePlanDraft(bad_draft_plan);
        const JournalDraftResult journal_draft = ValidateJournalDraft("", "msft", "Review", "reason", "", "tag", "", "A", 10.0);
        const JournalDraftResult bad_journal_draft = ValidateJournalDraft("", "msft", "", "reason", "", "tag", "", "A", 10.0);
        runner.Check(trade_plan_draft.ok && trade_plan_draft.row.symbol == "AAPL", "Workflow validation accepts trade plan drafts");
        runner.Check(!bad_trade_plan_draft.ok, "Workflow validation rejects invalid trade plan risk levels");
        runner.Check(journal_draft.ok && journal_draft.row.symbol == "MSFT", "Workflow validation accepts journal drafts");
        runner.Check(!bad_journal_draft.ok, "Workflow validation rejects journal drafts with missing action");

        AppendDiagnosticEvent({ runner.failed == 0 ? "info" : "error", "self-test", "RunSelfTests", "", runner.failed == 0 ? "passed" : "failed", runner.report.str(), "", 0, 0, false });

        std::error_code ec;
        std::filesystem::create_directories(AppDataDirectory(), ec);
        std::ofstream file(ReportPath(), std::ios::trunc);
        if (file)
        {
            file << "Aegis Stock Betting AI self-test\n";
            file << "Passed: " << runner.passed << "\n";
            file << "Failed: " << runner.failed << "\n\n";
            file << runner.report.str();
        }

        return runner.failed == 0 ? 0 : 2;
    }
}
