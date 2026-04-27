#include "SelfTests.h"

#include "AdvancedAnalytics.h"
#include "AlertEngine.h"
#include "AppSession.h"
#include "AppStorage.h"
#include "BrokerImport.h"
#include "Diagnostics.h"
#include "Json.h"
#include "ProviderLayer.h"
#include "ReportExport.h"
#include "SettingsService.h"
#include "StockData.h"

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

        const JsonParseResult json = ParseJson("{\"ok\":true,\"n\":42,\"rows\":[{\"symbol\":\"AAPL\"}]}");
        runner.Check(json.ok && json.value["ok"].AsBool(false) && json.value["n"].AsInt() == 42, "JSON parser handles object/bool/number");
        runner.Check(json.ok && json.value["rows"].At(0)["symbol"].AsString() == "AAPL", "JSON parser handles nested arrays");

        const StockQuote quote = TestQuote();
        std::vector<Candle> candles = BuildSyntheticCandles(quote, 120);
        runner.Check(candles.size() == 120 && candles.back().close > 0.0, "Synthetic candle builder returns usable rows");

        const IndicatorSnapshot indicators = BuildIndicators(candles, nullptr, nullptr);
        runner.Check(indicators.sma20 > 0.0 && indicators.sma50 > 0.0, "Indicators calculate moving averages");
        runner.Check(indicators.rsi14 >= 0.0 && indicators.rsi14 <= 100.0, "Indicators calculate RSI range");

        const BacktestResult backtest = RunSignalBacktest(candles, "Momentum");
        runner.Check(backtest.trades >= 0 && backtest.win_rate >= 0.0, "Built-in backtest returns sane stats");

        const StrategyBacktestResult strategy = RunStrategyRuleBacktest(candles, "RSI < 80 AND CLOSE > SMA50", 1.0, 2.0);
        runner.Check(strategy.ok && strategy.backtest.trades >= 0, "Strategy builder parses AND rules");

        const std::string redacted = RedactSecrets("https://example.test/query?apikey=SECRET123&symbol=AAPL password=hunter2");
        runner.Check(redacted.find("SECRET123") == std::string::npos && redacted.find("hunter2") == std::string::npos, "Diagnostics redact API keys and passwords");

        PriceAlert alert;
        alert.symbol = "TEST";
        alert.trigger_price = 100.0;
        alert.above = true;
        alert.enabled = true;
        std::vector<AlertEvent> events;
        events.push_back(MakeAlertEvent(alert, quote));
        runner.Check(IsAlertEventSuppressed(alert, events), "Alert engine suppresses duplicate open events");
        AcknowledgeAlertEvent(events.front());
        runner.Check(AlertEventState(events.front()) == "Acknowledged", "Alert engine acknowledges events");
        SnoozeAlertEvent(events.front(), 5);
        runner.Check(AlertEventState(events.front()) == "Snoozed", "Alert engine snoozes events");

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
        const std::vector<BrokerImportRow> broker_rows = PreviewBrokerImport(broker_dir);
        runner.Check(CountValidBrokerRows(broker_rows) == 1, "Broker import accepts one valid equity row");
        runner.Check(broker_rows.size() == 3, "Broker import keeps preview/error rows");

        StockSignal signal;
        signal.symbol = quote.symbol;
        signal.company = quote.name;
        signal.rating = "Watch";
        signal.posture = "Test posture";
        signal.score = 72;
        signal.confidence = 68;
        signal.target_price = 135.0;
        signal.stop_level = 112.0;
        signal.thesis = "Self-test report thesis.";
        SymbolResearchReport symbol_report;
        symbol_report.quote = quote;
        symbol_report.signal = signal;
        symbol_report.indicators = indicators;
        symbol_report.backtest = backtest;
        symbol_report.fundamentals = BuildFundamentals(quote);
        symbol_report.explanation = BuildModelExplanation(quote, signal, indicators);
        symbol_report.checklist = BuildTradeChecklist(quote, signal, indicators);
        symbol_report.risk_critic = BuildRiskCritic(quote, signal, indicators);
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
        saved_session.chart_days = 999;
        saved_session.strategy_rule = "RSI < 35 AND CLOSE > SMA50";
        runner.Check(SaveAppSessionState(saved_session, session_path), "Session store writes TSV state");
        const AppSessionState loaded_session = LoadAppSessionState(session_path);
        runner.Check(loaded_session.loaded && loaded_session.selected_tab == 9 && loaded_session.selected_symbol == "MSFT", "Session store normalizes tab and symbol");
        runner.Check(loaded_session.compare_symbols == "AAPL,MSFT" && loaded_session.chart_days == 365, "Session store normalizes basket and chart window");

        PersistentAppData storage_rows;
        storage_rows.holdings.push_back({ "AAPL", 1.0, 100.0, "Test holding" });
        storage_rows.price_alerts.push_back(alert);
        storage_rows.alert_events = events;
        storage_rows.trade_plans.push_back({ "now", "AAPL", "Watch", 100.0, 95.0, 110.0, 1, 5.0, 10.0, "Test thesis", "Open" });
        storage_rows.symbol_notes.push_back({ "AAPL", "test", "note", "now" });
        storage_rows.journal_entries.push_back({ "now", "AAPL", "Entry", "reason", "exit", "tag", "mistake", "A", 12.0 });
        const std::vector<InfoItem> storage_summary = BuildPersistentDataRows(storage_rows);
        runner.Check(storage_summary.size() == 6 && storage_summary.front().value == "1", "Storage facade summarizes persistent app data");

        Config settings_config;
        settings_config.refresh_seconds = 1;
        settings_config.max_symbols = 999;
        settings_config.alpha_quote_ttl_seconds = 1;
        SettingsInput settings_input;
        settings_input.watchlist = "aapl, msft, aapl";
        settings_input.auth_base_url = "";
        settings_input.sec_user_agent = "";
        settings_input.ui_compact_mode = true;
        settings_config = ApplySettingsInput(settings_config, settings_input);
        runner.Check(settings_config.watchlist == "AAPL,MSFT" && settings_config.auth_base_url == "http://127.0.0.1:8000", "Settings service normalizes watchlist and auth URL");
        runner.Check(settings_config.refresh_seconds == 30 && settings_config.max_symbols == 50 && settings_config.alpha_quote_ttl_seconds == 15, "Settings service clamps provider settings");
        runner.Check(BuildSettingsHealthRows(settings_config).size() == 6, "Settings service builds health rows");

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
