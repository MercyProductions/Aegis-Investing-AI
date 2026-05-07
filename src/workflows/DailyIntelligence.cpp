#include "DailyIntelligence.h"

#include "../ai/ResearchNarrative.h"
#include "../engine/AnalyticsEngine.h"
#include "../engine/PortfolioEngine.h"
#include "../engine/RiskEngine.h"
#include "../engine/SignalEngine.h"
#include "../Platform.h"
#include "../ProviderLayer.h"
#include "../services/SafetyGate.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace aegis
{
    namespace
    {
        InfoItem Row(const std::string& name, const std::string& label, const std::string& value, const std::string& detail, const std::string& state = "Ready", const std::string& tag = "daily-intel")
        {
            InfoItem item;
            item.name = name;
            item.label = label;
            item.value = value;
            item.detail = detail;
            item.state = state;
            item.tag = tag;
            return item;
        }

        double HoldingValue(const StockState& state, const PortfolioHolding& holding)
        {
            const StockQuote* quote = FindQuote(state, holding.symbol);
            const double price = quote != nullptr && quote->price > 0.0 ? quote->price : holding.average_cost;
            return holding.shares * price;
        }

        double TotalExposure(const StockState& state, const std::vector<PortfolioHolding>& holdings)
        {
            double total = 0.0;
            for (const PortfolioHolding& holding : holdings)
                total += HoldingValue(state, holding);
            return total;
        }

        int AlertsNearTrigger(const StockState& state, const std::vector<PriceAlert>& alerts)
        {
            int count = 0;
            for (const PriceAlert& alert : alerts)
            {
                if (!alert.enabled || alert.trigger_price <= 0.0)
                    continue;
                const StockQuote* quote = FindQuote(state, alert.symbol);
                if (quote == nullptr || quote->price <= 0.0)
                    continue;
                const double distance = std::fabs(quote->price - alert.trigger_price) / alert.trigger_price;
                if (distance <= 0.02)
                    ++count;
            }
            return count;
        }

        std::vector<StockQuote> RankedMovers(const StockState& state)
        {
            std::vector<StockQuote> movers = state.quotes;
            std::sort(movers.begin(), movers.end(), [](const StockQuote& left, const StockQuote& right) {
                return std::fabs(left.change_percent) > std::fabs(right.change_percent);
            });
            return movers;
        }

        std::vector<StockSignal> RankedSignals(const StockState& state)
        {
            std::vector<StockSignal> signals = state.signals;
            std::sort(signals.begin(), signals.end(), [](const StockSignal& left, const StockSignal& right) {
                return left.score > right.score;
            });
            return signals;
        }

        std::string SourceDetail(const StockState& state)
        {
            return "Source: " + (state.source_badge.empty() ? std::string("unlabeled") : state.source_badge) +
                ". Freshness: " + (state.last_refresh_label.empty() ? std::string("--") : state.last_refresh_label) +
                ". " + (state.source_label.empty() ? std::string("Provider metadata should be checked before acting.") : state.source_label);
        }

        std::string AlertCoverageLabel(const std::vector<PriceAlert>& alerts, const std::string& symbol)
        {
            const bool covered = std::any_of(alerts.begin(), alerts.end(), [&](const PriceAlert& alert) {
                return alert.enabled && Upper(alert.symbol) == Upper(symbol);
            });
            return covered ? "Covered" : "No alert";
        }
    }

    const char* DailyFocusModeName(DailyFocusMode mode)
    {
        switch (mode)
        {
        case DailyFocusMode::PortfolioMonitor: return "Portfolio Monitor";
        case DailyFocusMode::WatchlistScanner: return "Watchlist Scanner";
        case DailyFocusMode::EarningsMode: return "Earnings Mode";
        case DailyFocusMode::VolatilityMode: return "Volatility Mode";
        case DailyFocusMode::PaperExecutionMode: return "Paper Execution Mode";
        case DailyFocusMode::ResearchMode: return "Research Mode";
        default: return "Research Mode";
        }
    }

    const char* PortfolioRiskStateName(PortfolioRiskState state)
    {
        switch (state)
        {
        case PortfolioRiskState::Healthy: return "Healthy";
        case PortfolioRiskState::Caution: return "Caution";
        case PortfolioRiskState::Elevated: return "Elevated";
        case PortfolioRiskState::Defensive: return "Defensive";
        case PortfolioRiskState::Critical: return "Critical";
        default: return "Caution";
        }
    }

    PortfolioRiskState ClassifyPortfolioRiskState(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<PriceAlert>& alerts)
    {
        const RiskSummary risk = BuildRiskSummary(state, holdings, config.portfolio_cash);
        const int near_alerts = AlertsNearTrigger(state, alerts);
        const bool guardrails_off = !config.paper_only_mode || !config.require_manual_confirmation;
        if (guardrails_off || risk.concentration > 45.0 || risk.estimated_drawdown < -28.0)
            return PortfolioRiskState::Critical;
        if (risk.concentration > 35.0 || risk.estimated_drawdown < -22.0)
            return PortfolioRiskState::Defensive;
        if (risk.concentration > 25.0 || near_alerts >= 3)
            return PortfolioRiskState::Elevated;
        if (holdings.empty() || near_alerts > 0)
            return PortfolioRiskState::Caution;
        return PortfolioRiskState::Healthy;
    }

    std::vector<InfoItem> BuildMorningBriefingRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<PriceAlert>& alerts, const std::vector<AlertEvent>& events, const std::vector<TradePlan>& plans)
    {
        std::vector<InfoItem> rows;
        const std::vector<StockQuote> movers = RankedMovers(state);
        const std::vector<StockSignal> signals = RankedSignals(state);
        const PortfolioRiskState risk_state = ClassifyPortfolioRiskState(config, state, holdings, alerts);
        const int near_alerts = AlertsNearTrigger(state, alerts);
        const int unread_alerts = CountUnreadAlertEvents(events);

        rows.push_back(Row("Market Overview", state.market_status.empty() ? "Unknown" : state.market_status, state.last_refresh_label.empty() ? "--" : state.last_refresh_label, state.market_detail.empty() ? SourceDetail(state) : state.market_detail + " " + SourceDetail(state), state.loaded_from_api ? "Ready" : "Review"));
        rows.push_back(Row("What changed overnight", movers.empty() ? "No movers" : movers.front().symbol, movers.empty() ? "--" : FormatPercent(movers.front().change_percent), movers.empty() ? "No watchlist movement is loaded yet." : movers.front().name + " moved most in the current board. Verify provider freshness before using the move as research context.", movers.empty() ? "Review" : "Ready"));
        rows.push_back(Row("What to watch today", signals.empty() ? "No candidates" : signals.front().symbol, signals.empty() ? "--" : std::to_string(signals.front().score), signals.empty() ? "No scanner output is currently loaded." : signals.front().thesis + " Manual confirmation remains required.", signals.empty() ? "Review" : "Ready"));
        rows.push_back(Row("Portfolio Risk Check", PortfolioRiskStateName(risk_state), FormatCurrency(TotalExposure(state, holdings)), "Risk state is driven by exposure, concentration, alert proximity, and paper/manual safety settings.", risk_state >= PortfolioRiskState::Elevated ? "Review" : "Ready"));
        rows.push_back(Row("Holdings needing attention", near_alerts > 0 ? "Near alert" : "Clear", std::to_string(near_alerts), "Alerts within 2% of their trigger should be reviewed before market open.", near_alerts > 0 ? "Review" : "Ready"));
        rows.push_back(Row("Earnings/Event Calendar", "Manual review", "Today", "Earnings and SEC/news events should be checked for watchlist symbols before creating paper plans.", "Review"));
        rows.push_back(Row("News/SEC Highlights", state.loaded_from_api ? "Provider" : "Fallback", state.source_badge, "Research bundles and SEC highlights must show source/freshness on symbol pages.", state.loaded_from_api ? "Ready" : "Review"));
        rows.push_back(Row("AI Research Notes", signals.empty() ? "Waiting" : "Available", std::to_string(static_cast<int>(signals.size())), "Each note should explain why the signal exists, what changed, and what would invalidate the setup."));
        rows.push_back(Row("Today's Paper Trade Candidates", signals.empty() ? "None" : "Ranked", std::to_string(std::min<int>(3, static_cast<int>(signals.size()))), "Candidates are paper/research plans only and require manual confirmation."));
        rows.push_back(Row("Risk Warnings", unread_alerts > 0 ? "Open notifications" : "No unread alerts", std::to_string(unread_alerts), "Unread alerts, stale data, provider degradation, or elevated risk state should block rushed decisions.", unread_alerts > 0 ? "Review" : "Ready"));
        rows.push_back(Row("Action Checklist", "Before open", "9 checks", "Confirm data freshness, provider health, market regime, holdings risk, alert proximity, earnings/events, research notes, paper candidates, and journal plan."));
        (void)plans;
        return rows;
    }

    std::vector<InfoItem> BuildMarketCommandCenterRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<PriceAlert>& alerts, const std::vector<AlertEvent>& events, DailyFocusMode mode)
    {
        const std::vector<StockSignal> signals = RankedSignals(state);
        const std::vector<StockQuote> movers = RankedMovers(state);
        const PortfolioRiskState risk_state = ClassifyPortfolioRiskState(config, state, holdings, alerts);
        std::vector<InfoItem> rows = {
            Row("Global market ribbon", state.market_status.empty() ? "Unknown" : state.market_status, state.last_refresh_label.empty() ? "--" : state.last_refresh_label, "Shows market session, provider freshness, fallback state, and risk mode."),
            Row("Focus mode", DailyFocusModeName(mode), "Workspace", "Panels rearrange around the selected operating mode: portfolio, scanner, earnings, volatility, paper execution, or research."),
            Row("Active watchlist", state.quotes.empty() ? "Empty" : "Loaded", std::to_string(static_cast<int>(state.quotes.size())), "Watchlist rows are sorted and filtered without hiding data-quality labels.", state.quotes.empty() ? "Review" : "Ready"),
            Row("Scanner stream", signals.empty() ? "Quiet" : "Active", std::to_string(static_cast<int>(signals.size())), signals.empty() ? "No signals loaded." : "Top stream item: " + signals.front().symbol + " because " + signals.front().thesis),
            Row("Triggered alerts", CountUnreadAlertEvents(events) > 0 ? "Unread" : "Clear", std::to_string(CountUnreadAlertEvents(events)), "Alert events should link to notification inbox, symbol page, and audit event.", CountUnreadAlertEvents(events) > 0 ? "Review" : "Ready"),
            Row("Selected-symbol chart", state.selected_symbol.empty() ? "No selection" : state.selected_symbol, "Chart Lab", "Command center keeps the selected symbol tied to chart, signal, alerts, notes, and paper queue."),
            Row("Position exposure", holdings.empty() ? "No holdings" : "Tracked", FormatCurrency(TotalExposure(state, holdings)), "Exposure panel uses current marks with cost-basis fallback."),
            Row("Paper order queue", "Guarded", config.require_manual_confirmation ? "Manual required" : "Review", "Paper execution mode displays queued, filled, rejected, cancelled, and expired paper orders only.", config.require_manual_confirmation ? "Ready" : "Review"),
            Row("Risk guardrail panel", PortfolioRiskStateName(risk_state), FormatCurrency(config.max_position_amount), "Risk state explains concentration, drawdown, cash, stop coverage, and alert coverage.", risk_state >= PortfolioRiskState::Elevated ? "Review" : "Ready"),
            Row("Provider health", state.loaded_from_api ? "Provider" : "Fallback", state.source_badge, SourceDetail(state), state.loaded_from_api ? "Ready" : "Review"),
            Row("Market session timer", "Planned", state.market_status.empty() ? "Unknown" : state.market_status, "Session countdown belongs in MarketEngine once exchange calendar support is added.")
        };
        if (!movers.empty())
            rows.push_back(Row("Watchlist heat", movers.front().symbol, FormatPercent(movers.front().change_percent), "Largest watchlist move in this refresh cycle."));
        return rows;
    }

    std::vector<InfoItem> BuildEndOfDayReviewRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<AlertEvent>& events, const std::vector<TradePlan>& plans, const std::vector<JournalEntry>& journal)
    {
        const PortfolioEngineSnapshot portfolio = BuildPortfolioEngineSnapshot(state, holdings, config.portfolio_cash);
        int closed = 0;
        int open = 0;
        for (const TradePlan& plan : plans)
        {
            if (Lower(plan.status).find("closed") != std::string::npos)
                ++closed;
            else
                ++open;
        }
        int mistake_notes = 0;
        for (const JournalEntry& entry : journal)
        {
            if (!Trim(entry.mistakes).empty())
                ++mistake_notes;
        }
        return {
            Row("Portfolio P/L", portfolio.unrealized_pnl >= 0.0 ? "Positive" : "Negative", FormatCurrency(portfolio.unrealized_pnl), "End-of-day review compares marked paper exposure against cost basis."),
            Row("Best/worst paper positions", "Review", std::to_string(static_cast<int>(holdings.size())), "Position ranking should be reviewed for best and weakest paper holdings."),
            Row("Paper order lifecycle", "Audit", std::to_string(static_cast<int>(plans.size())) + " plans", "Filled, rejected, cancelled, and expired paper orders should be summarized from the execution audit stream."),
            Row("Signal accuracy", "Tracked", std::to_string(closed) + " closed", "Signals should be scored against outcomes without hindsight edits."),
            Row("Alerts triggered", CountUnreadAlertEvents(events) > 0 ? "Unread" : "Clear", std::to_string(static_cast<int>(events.size())), "Triggered alerts and snoozes should be reviewed before tomorrow's watchlist.", CountUnreadAlertEvents(events) > 0 ? "Review" : "Ready"),
            Row("Missed opportunities", "Journal", std::to_string(open) + " open plans", "Compare high-score candidates against paper plans that were not acted on."),
            Row("Risk violations", "Guardrails", CurrentRiskStateLabel(config), "Any paper execution outside risk limits should create a journal prompt and audit entry."),
            Row("Mistake review", mistake_notes > 0 ? "Review" : "Clear", std::to_string(mistake_notes), "Journal mistakes should become tomorrow's checklist items.", mistake_notes > 0 ? "Review" : "Ready"),
            Row("Tomorrow notes", "Generated", "Watchlist", "EOD workflow should generate tomorrow watchlist, journal prompts, and strategy performance update.")
        };
    }

    std::vector<InfoItem> BuildWeeklyReviewRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<TradePlan>& plans, const std::vector<JournalEntry>& journal)
    {
        const RiskSummary risk = BuildRiskSummary(state, holdings, config.portfolio_cash);
        const PortfolioRiskState risk_state = ClassifyPortfolioRiskState(config, state, holdings, {});
        int repeated_mistakes = 0;
        std::map<std::string, int> mistake_counts;
        for (const JournalEntry& entry : journal)
        {
            if (!Trim(entry.mistakes).empty())
                repeated_mistakes = std::max(repeated_mistakes, ++mistake_counts[Lower(entry.mistakes)]);
        }
        return {
            Row("Portfolio health grade", PortfolioRiskStateName(risk_state), risk_state <= PortfolioRiskState::Caution ? "A/B" : risk_state == PortfolioRiskState::Elevated ? "C" : "D", "Grade combines exposure, concentration, estimated drawdown, cash drag, and guardrail state.", risk_state >= PortfolioRiskState::Elevated ? "Review" : "Ready"),
            Row("Total return", "Paper", "Tracked", "Weekly report should calculate realized plus unrealized paper return against starting equity."),
            Row("Benchmark comparison", "Planned", "SPY/QQQ", "Benchmark comparison will use HistoricalStore once benchmark candles are centralized."),
            Row("Drawdown", risk.estimated_drawdown < -20.0 ? "Elevated" : "Contained", FormatPercent(risk.estimated_drawdown), "Weekly drawdown scenario from beta and concentration."),
            Row("Volatility", "Estimated", std::to_string(static_cast<int>(risk.beta_exposure * 100.0) / 100.0), "Beta exposure proxies volatility until return-based portfolio beta is fully wired."),
            Row("Sector/concentration", risk.concentration > 25.0 ? "Review" : "Contained", FormatPercent(risk.concentration), "Largest holding concentration should drive rebalance suggestions.", risk.concentration > 25.0 ? "Review" : "Ready"),
            Row("Cash allocation", "Paper", FormatPercent(risk.cash_drag), "Cash allocation should be intentional, not accidental."),
            Row("Strategy performance", plans.empty() ? "No plans" : "Tracked", std::to_string(static_cast<int>(plans.size())), "Weekly workflow updates paper strategy performance and strongest/weakest setups."),
            Row("Repeated journal mistakes", repeated_mistakes > 1 ? "Pattern" : "None", std::to_string(repeated_mistakes), "Repeated mistakes should become next-week checklist warnings.", repeated_mistakes > 1 ? "Review" : "Ready"),
            Row("Next-week watchlist", "Generated", std::to_string(static_cast<int>(state.signals.size())), "Next-week list should prefer high-quality, well-labeled data and clear catalysts.")
        };
    }

    std::vector<InfoItem> BuildStrategyLabWorkflowRows(const Config& config, const StockState& state, const std::vector<TradePlan>& plans, const std::vector<Candle>& candles)
    {
        const BacktestResult backtest = RunSignalBacktest(candles, "Momentum");
        const AnalyticsEngineSnapshot analytics = BuildAnalyticsEngineSnapshot(candles, "Strategy Lab");
        return {
            Row("Strategy creation", "Ready", "Rule builder", "Create/edit strategy presets using moving averages, RSI, MACD, volume spikes, gaps, earnings windows, volatility filters, benchmark filters, regime filters, risk limits, stops, and targets."),
            Row("Current backtest", backtest.trades > 0 ? "Available" : "No trades", std::to_string(backtest.trades), "Backtests use closed candle data and are research tools, not prediction guarantees.", backtest.trades > 0 ? "Ready" : "Review"),
            Row("Win rate", "Backtest", FormatPercent(backtest.win_rate), "Win rate must be read alongside drawdown, trade count, and average return."),
            Row("Average return", "Backtest", FormatPercent(backtest.average_return), "Average return is historical simulation output only."),
            Row("Max drawdown", backtest.max_drawdown < -15.0 ? "Review" : "Contained", FormatPercent(backtest.max_drawdown), "Strategy drawdown should constrain paper sizing.", backtest.max_drawdown < -15.0 ? "Review" : "Ready"),
            Row("Sharpe/Sortino", "Estimated", std::to_string(static_cast<int>(analytics.sharpe * 100.0) / 100.0) + " / " + std::to_string(static_cast<int>(analytics.sortino * 100.0) / 100.0), "Risk-adjusted estimates use the loaded candle window."),
            Row("Paper performance", plans.empty() ? "No saved plans" : "Tracked", std::to_string(static_cast<int>(plans.size())), "Saved paper plans become the live paper-performance ledger for strategy presets."),
            Row("Risk limits", "Required", FormatCurrency(config.max_position_amount), "Every strategy should declare allocation cap, stop/target logic, and invalidation conditions."),
            Row("Active signals", state.signals.empty() ? "None" : "Available", std::to_string(static_cast<int>(state.signals.size())), "Current active signals should explain why they exist and what would invalidate them.")
        };
    }

    std::vector<InfoItem> BuildWatchlistIntelligenceRows(const StockState& state, const std::vector<PriceAlert>& alerts, const std::vector<SymbolNote>& notes)
    {
        std::vector<InfoItem> rows;
        const std::vector<StockSignal> signals = RankedSignals(state);
        for (size_t i = 0; i < signals.size() && i < 8; ++i)
        {
            const StockSignal& signal = signals[i];
            const StockQuote* quote = FindQuote(state, signal.symbol);
            const bool has_note = std::any_of(notes.begin(), notes.end(), [&](const SymbolNote& note) { return Upper(note.symbol) == Upper(signal.symbol); });
            const int liquidity = quote != nullptr && quote->volume > 1000000 ? 80 : 45;
            const int volatility = quote != nullptr && std::fabs(quote->change_percent) > 2.0 ? 70 : 50;
            const int risk = 100 - DataConfidencePenalty(quote != nullptr ? *quote : StockQuote{});
            const int overall = std::clamp((signal.score + signal.confidence + liquidity + risk - std::max(0, volatility - 65)) / 4, 0, 100);
            rows.push_back(Row(signal.symbol, AlertCoverageLabel(alerts, signal.symbol), std::to_string(overall), "Technical " + std::to_string(signal.score) + ", sentiment/model confidence " + std::to_string(signal.confidence) + ", liquidity " + std::to_string(liquidity) + ", volatility " + std::to_string(volatility) + ", notes " + std::string(has_note ? "present" : "missing") + ". " + signal.thesis, overall >= 70 ? "Ready" : "Review", "watchlist-intel"));
        }
        if (rows.empty())
            rows.push_back(Row("Watchlist intelligence", "Waiting", "--", "No signal rows are loaded yet.", "Review", "watchlist-intel"));
        return rows;
    }

    std::vector<InfoItem> BuildPortfolioRiskConsoleRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<PriceAlert>& alerts)
    {
        const RiskSummary risk = BuildRiskSummary(state, holdings, config.portfolio_cash);
        const PortfolioRiskState risk_state = ClassifyPortfolioRiskState(config, state, holdings, alerts);
        const double exposure = TotalExposure(state, holdings);
        int stop_coverage = 0;
        for (const PortfolioHolding& holding : holdings)
        {
            if (std::any_of(alerts.begin(), alerts.end(), [&](const PriceAlert& alert) { return alert.enabled && Upper(alert.symbol) == Upper(holding.symbol); }))
                ++stop_coverage;
        }
        const double coverage = holdings.empty() ? 0.0 : static_cast<double>(stop_coverage) / holdings.size() * 100.0;
        return {
            Row("Risk state", PortfolioRiskStateName(risk_state), CurrentRiskStateLabel(config), "Risk state explains exposure, concentration, alert coverage, drawdown, cash, and safety guards.", risk_state >= PortfolioRiskState::Elevated ? "Review" : "Ready", "risk-console"),
            Row("Total exposure", holdings.empty() ? "Empty" : "Tracked", FormatCurrency(exposure), "Marked exposure across paper holdings.", "Ready", "risk-console"),
            Row("Cash allocation", "Paper", FormatPercent(risk.cash_drag), "Cash as a percentage of paper portfolio plus cash.", "Ready", "risk-console"),
            Row("Position concentration", risk.concentration > 25.0 ? "Review" : "Contained", FormatPercent(risk.concentration), "Largest position as percent of marked holdings.", risk.concentration > 25.0 ? "Review" : "Ready", "risk-console"),
            Row("Sector concentration", "Estimated", "See exposure", "Sector exposure uses quote sectors and should be expanded into heatmaps.", "Ready", "risk-console"),
            Row("Correlated holdings", "Estimated", "Matrix", "Correlation matrix and ETF overlap should be reviewed when holdings cluster together.", "Ready", "risk-console"),
            Row("Beta", risk.beta_exposure > 1.25 ? "High" : "Normal", std::to_string(static_cast<int>(risk.beta_exposure * 100.0) / 100.0), "Weighted beta exposure across holdings.", risk.beta_exposure > 1.25 ? "Review" : "Ready", "risk-console"),
            Row("Drawdown scenario", risk.estimated_drawdown < -20.0 ? "Elevated" : "Contained", FormatPercent(risk.estimated_drawdown), "Maximum simulated loss scenario from beta and concentration.", risk.estimated_drawdown < -20.0 ? "Review" : "Ready", "risk-console"),
            Row("Stop/alert coverage", coverage < 70.0 && !holdings.empty() ? "Incomplete" : "Covered", FormatPercent(coverage), "Holdings should have alert or stop coverage before larger paper allocations.", coverage < 70.0 && !holdings.empty() ? "Review" : "Ready", "risk-console"),
            Row("Risk per paper trade", "Configured", FormatCurrency(config.max_position_amount), "Risk per paper trade should remain inside position and portfolio limits.", "Ready", "risk-console")
        };
    }

    std::vector<InfoItem> BuildSymbolResearchPageRows(const Config& config, const StockState& state, const std::string& symbol, const std::vector<PortfolioHolding>& holdings, const std::vector<PriceAlert>& alerts, const std::vector<TradePlan>& plans, const std::vector<SymbolNote>& notes, const std::vector<JournalEntry>& journal)
    {
        const StockQuote* quote = FindQuote(state, symbol);
        const StockSignal* signal = FindSignal(state, symbol);
        std::vector<InfoItem> rows;
        if (quote == nullptr)
        {
            rows.push_back(Row("Symbol Research", "Not found", symbol, "Select a symbol from the watchlist or scanner.", "Review", "symbol-research"));
            return rows;
        }
        const bool held = std::any_of(holdings.begin(), holdings.end(), [&](const PortfolioHolding& holding) { return Upper(holding.symbol) == Upper(symbol); });
        const bool has_alert = AlertCoverageLabel(alerts, symbol) == "Covered";
        const bool has_plan = std::any_of(plans.begin(), plans.end(), [&](const TradePlan& plan) { return Upper(plan.symbol) == Upper(symbol); });
        const bool has_note = std::any_of(notes.begin(), notes.end(), [&](const SymbolNote& note) { return Upper(note.symbol) == Upper(symbol); });
        const bool has_journal = std::any_of(journal.begin(), journal.end(), [&](const JournalEntry& entry) { return Upper(entry.symbol) == Upper(symbol); });
        rows.push_back(Row("Overview", quote->name, FormatCurrency(quote->price), "Company/security context, sector, source, and latest price. " + SourceDetail(state), quote->live ? "Ready" : "Review", "symbol-research"));
        rows.push_back(Row("Why moving", quote->change_percent >= 0.0 ? "Up" : "Down", FormatPercent(quote->change_percent), "Move may indicate momentum, news, sector pressure, or broad market beta. Fresh news/SEC review is still needed.", "Ready", "symbol-research"));
        rows.push_back(Row("Trend", signal != nullptr ? signal->rating : "No signal", signal != nullptr ? std::to_string(signal->score) : "--", signal != nullptr ? signal->thesis : "Scanner has not produced a signal for this symbol.", signal != nullptr ? "Ready" : "Review", "symbol-research"));
        rows.push_back(Row("Risks", signal != nullptr ? signal->risk : "Manual", FormatVolume(quote->volume), "Risk review should include volatility, liquidity, earnings, SEC events, and portfolio concentration.", "Review", "symbol-research"));
        rows.push_back(Row("Events coming", "Calendar", "Check", "Earnings and catalyst proximity should be confirmed before paper plans.", "Review", "symbol-research"));
        rows.push_back(Row("Portfolio impact", held ? "Held" : "Watchlist", held ? "Exposure" : "No position", "Portfolio impact depends on current holding, sector overlap, beta, and alert coverage.", held ? "Ready" : "Review", "symbol-research"));
        rows.push_back(Row("Paper trade plan", has_plan ? "Saved" : "Draft needed", config.require_manual_confirmation ? "Manual required" : "Review", "Reasonable paper plans should include entry, stop, target, sizing, invalidation, and audit note.", has_plan ? "Ready" : "Review", "symbol-research"));
        rows.push_back(Row("Notes/Journal/Alerts", has_note || has_journal || has_alert ? "Context exists" : "Missing context", std::string(has_note ? "note " : "") + std::string(has_journal ? "journal " : "") + std::string(has_alert ? "alert" : ""), "Symbol page should link notes, journal, paper trades, alerts, filings, news, and ETF exposure.", has_note || has_journal || has_alert ? "Ready" : "Review", "symbol-research"));
        return rows;
    }

    std::vector<InfoItem> BuildAiExplanationLayerRows(const StockState& state, const std::string& symbol)
    {
        const StockSignal* signal = FindSignal(state, symbol);
        if (signal == nullptr)
            return { Row("AI explanation", "No signal", symbol, "Select a symbol with a scanner signal to explain why it exists.", "Review", "ai-explain") };
        return {
            Row("Why this signal?", signal->rating, std::to_string(signal->score), signal->thesis.empty() ? "The score suggests conditions may be improving, but confirmation is still needed." : signal->thesis, "Ready", "ai-explain"),
            Row("Why this risk?", signal->risk, signal->downside, "Risk appears tied to volatility, liquidity, trend invalidation, and data quality.", "Ready", "ai-explain"),
            Row("What changed?", "Review", signal->horizon, "Recent price/volume and scanner factors may indicate a changing setup; verify source freshness.", "Ready", "ai-explain"),
            Row("Invalidation", "Required", FormatCurrency(signal->stop_level), "A break below the stop/thesis level may invalidate the paper setup.", "Review", "ai-explain"),
            Row("Watch next", "Confirmation needed", FormatCurrency(signal->target_price), "Watch follow-through, market regime, news, earnings proximity, and alert levels.", "Ready", "ai-explain")
        };
    }

    std::vector<InfoItem> BuildNotificationInboxRows(const Config& config, const StockState& state, const std::vector<AlertEvent>& events)
    {
        std::vector<InfoItem> rows = BuildAlertNotificationRows(events, state.quotes, 8);
        if (!config.paper_only_mode || !config.require_manual_confirmation)
            rows.insert(rows.begin(), Row("Risk threshold breached", "High", "Safety", "Paper-only or manual-confirmation setting requires review.", "Review", "notification"));
        if (!state.loaded_from_api)
            rows.insert(rows.begin(), Row("Data freshness", "Medium", state.source_badge, "Provider data is fallback/demo or stale. Analysis confidence should be reduced.", "Review", "notification"));
        if (rows.empty())
            rows.push_back(Row("Notification inbox", "Clear", "--", "No alert, report, provider, stale-data, risk, paper order, earnings, SEC, or strategy notifications are currently unread.", "Ready", "notification"));
        return rows;
    }

    std::vector<InfoItem> BuildCommandPaletteRows()
    {
        return {
            Row("Ctrl+K / command field", "Available", "Search", "Commands: search symbol, open dashboard, open strategy lab, open portfolio, create alert, create paper order, export report, run scanner, refresh providers, open settings, switch workspace, open journal.", "Ready", "command"),
            Row("Symbol search", "Command", "symbol AAPL", "Search/open a symbol research page."),
            Row("Create alert", "Command", "alert AAPL", "Jump to alert creation with selected symbol."),
            Row("Paper order", "Command", "paper AAPL", "Open paper execution workflow; manual confirmation remains required."),
            Row("Export report", "Command", "report", "Generate report from the active workflow and audit the export.")
        };
    }

    std::vector<InfoItem> BuildSavedLayoutRows()
    {
        return {
            Row("Default", "Layout", "Daily", "Balanced daily intelligence layout."),
            Row("Market Open", "Layout", "Command Center", "Global ribbon, watchlist, scanner stream, alerts, risk, and provider health."),
            Row("Research", "Layout", "Symbol Page", "Overview, chart, signals, fundamentals, earnings, news, SEC, options, backtests, ETF exposure, journal, paper trades, alerts."),
            Row("Portfolio Review", "Layout", "Risk Console", "Exposure, concentration, correlation, ETF overlap, beta, drawdown, stop coverage, and cash."),
            Row("Strategy Testing", "Layout", "Strategy Lab", "Rule builder, backtest, strategy comparison, paper performance, and active signals."),
            Row("Earnings Watch", "Layout", "Events", "Earnings proximity, volatility, news, and SEC highlight panels."),
            Row("Compact Laptop", "Layout", "Single column", "Prioritizes selected symbol, watchlist, alerts, and risk state."),
            Row("Multi-Monitor", "Layout", "Expanded", "Separates command center, chart lab, and portfolio/risk views.")
        };
    }

    std::vector<InfoItem> BuildImportExportWorkflowRows()
    {
        return {
            Row("CSV portfolio import", "Preview first", "Required", "Never mutate holdings without preview, validation, and confirmation.", "Ready", "import-export"),
            Row("Broker activity import", "Preview first", "Required", "Broker rows should map to paper journal/order history only after review.", "Ready", "import-export"),
            Row("Watchlist import/export", "Supported path", "CSV", "Watchlists should preserve groups, tags, notes, and alert coverage.", "Ready", "import-export"),
            Row("Strategy import/export", "Planned", "JSON", "Strategy presets should include rules, risk limits, stops, targets, and disabled/enabled state.", "Ready", "import-export"),
            Row("Settings backup", "Required", "Config", "Provider keys must remain redacted in exports unless explicitly backed up through secure storage.", "Ready", "import-export"),
            Row("Full workspace backup", "Required", "Archive", "Layouts, watchlists, notes, journal, reports, and settings should export with manifest and validation.")
        };
    }

    std::vector<InfoItem> BuildDataQualityScoreRows(const StockState& state, const std::string& symbol)
    {
        std::vector<const StockQuote*> quotes;
        if (!symbol.empty())
        {
            if (const StockQuote* quote = FindQuote(state, symbol))
                quotes.push_back(quote);
        }
        else
        {
            for (const StockQuote& quote : state.quotes)
                quotes.push_back(&quote);
        }

        int fresh = 0;
        int delayed = 0;
        int stale = 0;
        int fallback = 0;
        int failed = 0;
        for (const StockQuote* quote : quotes)
        {
            const std::string quality = Lower(quote->data_quality + " " + quote->status + " " + quote->note + " " + quote->source);
            if (quote->live && quality.find("stale") == std::string::npos && quality.find("fallback") == std::string::npos)
                ++fresh;
            if (quality.find("delayed") != std::string::npos)
                ++delayed;
            if (quality.find("stale") != std::string::npos)
                ++stale;
            if (!quote->live || quality.find("fallback") != std::string::npos || quality.find("demo") != std::string::npos)
                ++fallback;
            if (quality.find("error") != std::string::npos || quality.find("failed") != std::string::npos)
                ++failed;
        }
        const int total = static_cast<int>(quotes.size());
        const int score = total == 0 ? 0 : std::clamp((fresh * 100 + delayed * 70 + stale * 45 + fallback * 35 - failed * 40) / total, 0, 100);
        const std::string confidence = score >= 80 ? "High" : score >= 55 ? "Medium" : "Low";
        return {
            Row("Research confidence", confidence, std::to_string(score) + "/100", "Confidence reflects provider freshness, stale/fallback flags, and failures. Fundamentals/news may still need separate source checks.", score >= 55 ? "Ready" : "Review", "data-quality"),
            Row("Fresh", "Provider", std::to_string(fresh), "Provider-backed rows without stale/fallback labels.", "Ready", "data-quality"),
            Row("Delayed", "Provider", std::to_string(delayed), "Delayed rows reduce confidence but can still support research context.", delayed > 0 ? "Review" : "Ready", "data-quality"),
            Row("Stale", "Review", std::to_string(stale), "Stale rows should block paper execution decisions until refreshed.", stale > 0 ? "Review" : "Ready", "data-quality"),
            Row("Fallback/demo", "Review", std::to_string(fallback), "Fallback rows remain useful for UI testing and offline workflows but lower research confidence.", fallback > 0 ? "Review" : "Ready", "data-quality"),
            Row("Failed", "Error", std::to_string(failed), "Failed provider bundles should create notification and audit events.", failed > 0 ? "Review" : "Ready", "data-quality")
        };
    }

    std::vector<InfoItem> BuildOnboardingWorkflowRows(const Config& config, const StockState& state, const std::vector<PortfolioHolding>& holdings)
    {
        return {
            Row("Paper-only mode", config.paper_only_mode ? "Complete" : "Required", config.paper_only_mode ? "ON" : "OFF", "First run should make paper-only mode explicit.", config.paper_only_mode ? "Ready" : "Review", "onboarding"),
            Row("No financial advice", "Required", "Confirm", "User should confirm Auralith is a research cockpit, not financial advice.", "Ready", "onboarding"),
            Row("Default watchlist", state.quotes.empty() ? "Needed" : "Loaded", std::to_string(static_cast<int>(state.quotes.size())), "Starter watchlist should be reviewed and editable.", state.quotes.empty() ? "Review" : "Ready", "onboarding"),
            Row("Provider keys", config.alpha_vantage_api_key.empty() ? "Optional" : "Configured", config.alpha_vantage_api_key.empty() ? "Fallback active" : "Alpha key saved", "Provider configuration should clearly label freshness and fallback state.", config.alpha_vantage_api_key.empty() ? "Review" : "Ready", "onboarding"),
            Row("Workspace style", "Choose", "Daily/Research/Risk", "Workspace style seeds saved layouts and focus modes.", "Ready", "onboarding"),
            Row("Risk preferences", config.require_manual_confirmation ? "Guarded" : "Review", FormatCurrency(config.max_position_amount), "Risk preferences control paper sizing, maximum position, and confirmation requirements.", config.require_manual_confirmation ? "Ready" : "Review", "onboarding"),
            Row("First paper portfolio", holdings.empty() ? "Optional" : "Created", std::to_string(static_cast<int>(holdings.size())), "User can create/import holdings with preview and confirmation.", holdings.empty() ? "Review" : "Ready", "onboarding"),
            Row("Notifications", config.notifications_enabled ? "Enabled" : "Disabled", config.notifications_enabled ? "ON" : "OFF", "Alerts, provider degradation, stale data, report ready, and paper order notifications are optional.", "Ready", "onboarding")
        };
    }
}
