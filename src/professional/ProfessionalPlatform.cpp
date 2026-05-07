#include "ProfessionalPlatform.h"

#include "../persistence/CoreDatabase.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <sstream>

namespace aegis
{
    namespace
    {
        InfoItem Row(const std::string& name, const std::string& label, const std::string& value, const std::string& detail, const std::string& state = "Ready")
        {
            InfoItem item;
            item.name = name;
            item.label = label;
            item.value = value;
            item.detail = detail;
            item.state = state;
            item.tag = "professional-platform";
            item.source = "Auralith Professional Platform";
            return item;
        }

        double HoldingMarketValue(const StockState& state, const PortfolioHolding& holding)
        {
            if (const StockQuote* quote = FindQuote(state, holding.symbol))
                return quote->price * holding.shares;
            return holding.average_cost * holding.shares;
        }

        std::string DominantSector(const StockState& state, const std::vector<PortfolioHolding>& holdings)
        {
            std::map<std::string, double> sectors;
            for (const PortfolioHolding& holding : holdings)
            {
                const StockQuote* quote = FindQuote(state, holding.symbol);
                const std::string sector = quote != nullptr && !quote->sector.empty() ? quote->sector : "Unclassified";
                sectors[sector] += HoldingMarketValue(state, holding);
            }
            auto best = std::max_element(sectors.begin(), sectors.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
            return best == sectors.end() ? "No exposure" : best->first;
        }

        double ScenarioImpact(const StockState& state, const std::vector<PortfolioHolding>& holdings, double broad_shock, const std::string& sector_focus = "", double sector_shock = 0.0)
        {
            double impact = 0.0;
            for (const PortfolioHolding& holding : holdings)
            {
                const StockQuote* quote = FindQuote(state, holding.symbol);
                const double value = HoldingMarketValue(state, holding);
                const bool sector_match = quote != nullptr && !sector_focus.empty() && Lower(quote->sector).find(Lower(sector_focus)) != std::string::npos;
                impact += value * (sector_match ? sector_shock : broad_shock);
            }
            return impact;
        }

        std::string TrendLabel(const std::vector<float>& history, size_t lookback)
        {
            if (history.size() <= lookback || history.back() <= 0.0f)
                return "Insufficient";
            const float then = history[history.size() - lookback - 1];
            if (then <= 0.0f)
                return "Insufficient";
            const double change = (history.back() - then) / then;
            if (change > 0.035)
                return "Improving";
            if (change < -0.035)
                return "Weakening";
            return "Neutral";
        }
    }

    std::vector<InfoItem> BuildProfessionalArchitectureRows()
    {
        return {
            Row("Core", "Service", "AuralithCore.exe", "Long-running provider sync, persistence, AI memory, reports, audit, and background jobs."),
            Row("Desktop", "Terminal", "ImGui/DX11", "Primary professional research cockpit and paper portfolio workstation."),
            Row("Web", "Companion", "React/Express", "Secondary dashboard for monitoring, reports, alerts, and lightweight review."),
            Row("Shared", "Contracts", "Models + schemas", "Symbols, quotes, positions, alerts, reports, risk events, scanner outputs, and AI summaries converge here."),
            Row("Providers", "Adapters", "Priority + cache", "Provider routing owns freshness, fallback, retry, and health metadata."),
            Row("SDK", "Extension boundary", "Sandboxed", "Future scripts, plugins, scanners, indicators, providers, and report exporters use declared permissions."),
            Row("Installer", "Deployment", "Planned", "Signed builds, release channels, rollback, portable mode, and asset pipeline remain explicit deployment work.", "Review")
        };
    }

    std::vector<InfoItem> BuildVisualStrategyBuilderRows()
    {
        return {
            Row("Node graph editor", "Planned", "condition chains", "No-code strategy builder should compile graph nodes into strategy definitions.", "Review"),
            Row("Signal nodes", "Defined", "RSI/SMA/MACD/volume/gaps", "Indicator nodes should use historical data contracts and expose data quality confidence."),
            Row("Risk nodes", "Defined", "exposure/drawdown/stop/liquidity", "Risk gates remain research-first and paper-only by default."),
            Row("Portfolio nodes", "Defined", "allocation/concentration/cash", "Strategy rules can inspect paper portfolio state without mutating it directly."),
            Row("Compiler output", "Required", "strategy JSON", "Graph definitions should serialize to a deterministic, auditable strategy document."),
            Row("Execution bridge", "Paper only", "manual confirmation", "Builder output can feed backtests and paper simulations, not live execution.")
        };
    }

    std::vector<InfoItem> BuildMultiTimeframeIntelligenceRows(const StockState& state)
    {
        const StockQuote* quote = FindQuote(state, state.selected_symbol);
        if (quote == nullptr)
            return { Row("Selected symbol", "Missing", state.selected_symbol, "Select a symbol before multi-timeframe analysis.", "Review") };

        return {
            Row("Intraday context", TrendLabel(quote->history, 3), quote->symbol, "Short-term movement based on the latest available mini-series."),
            Row("Daily context", TrendLabel(quote->history, 7), quote->symbol, "Daily research posture should include provider freshness and volume confirmation."),
            Row("Weekly context", TrendLabel(quote->history, 20), quote->symbol, "Weekly context tempers short-term scanner signals."),
            Row("Monthly context", TrendLabel(quote->history, 60), quote->symbol, "Longer horizon trend remains cautious when history is limited.", quote->history.size() > 60 ? "Ready" : "Review"),
            Row("AI phrasing", "Cautious", "mixed horizons", "Example: short-term momentum may improve while longer-term trend remains weak.")
        };
    }

    std::vector<InfoItem> BuildResearchCollectionRows(const StockState& state)
    {
        int technology = 0;
        int defensive = 0;
        int etf = 0;
        int high_volatility = 0;
        for (const StockQuote& quote : state.quotes)
        {
            const std::string sector = Lower(quote.sector + " " + quote.name + " " + quote.symbol);
            if (sector.find("tech") != std::string::npos || sector.find("semiconductor") != std::string::npos)
                ++technology;
            if (sector.find("consumer staples") != std::string::npos || sector.find("health") != std::string::npos || quote.beta < 0.9)
                ++defensive;
            if (quote.symbol == "SPY" || quote.symbol == "QQQ" || quote.name.find("ETF") != std::string::npos)
                ++etf;
            if (quote.beta > 1.25 || std::abs(quote.change_percent) > 2.0)
                ++high_volatility;
        }
        return {
            Row("AI Stocks", "Collection", std::to_string(technology), "Grouped watchlist for AI and platform-sensitive equities."),
            Row("Semiconductors", "Collection", std::to_string(technology), "Semiconductor and hardware exposure can share scanners, reports, and sector-relative views."),
            Row("Defensive ETFs", "Collection", std::to_string(defensive + etf), "Defensive and ETF collections support risk-off reviews."),
            Row("Earnings Momentum", "Collection", "planned overlay", "Collections should attach earnings windows, reports, and AI summaries.", "Review"),
            Row("High Volatility", "Collection", std::to_string(high_volatility), "Volatility-sensitive symbols should carry stronger risk warnings."),
            Row("Macro Sensitive", "Collection", "planned overlay", "Future macro-event overlays can bind collections to scenario testing.", "Review")
        };
    }

    std::vector<InfoItem> BuildCorrelationExposureRows(const StockState& state, const std::vector<PortfolioHolding>& holdings)
    {
        const double total_value = std::accumulate(holdings.begin(), holdings.end(), 0.0, [&](double sum, const PortfolioHolding& holding) {
            return sum + HoldingMarketValue(state, holding);
        });
        const std::string dominant = DominantSector(state, holdings);
        return {
            Row("Correlation matrix", "Foundation", std::to_string(holdings.size()) + " positions", "Next step is rolling-return correlation from HistoricalStore."),
            Row("Dominant sector", total_value > 0.0 ? "Mapped" : "No holdings", dominant, "Sector dependency should drive concentration heatmaps and scenario warnings.", total_value > 0.0 ? "Ready" : "Review"),
            Row("ETF overlap", "Planned", "holdings + ETF constituents", "ETF overlap needs constituent data before exact overlap scoring.", "Review"),
            Row("Benchmark dependency", "Foundation", state.quotes.empty() ? "--" : "SPY/QQQ context", "Benchmark-relative charts and beta estimates should use shared Quote/Candle contracts."),
            Row("Volatility clustering", "Foundation", "historical candles", "Cluster detection belongs in Advanced Historical Intelligence.")
        };
    }

    std::vector<InfoItem> BuildScenarioSimulationRows(const StockState& state, const std::vector<PortfolioHolding>& holdings)
    {
        if (holdings.empty())
            return { Row("Scenario engine", "No positions", "0", "Create or import a paper portfolio before portfolio impact simulations.", "Review") };

        return {
            Row("Market correction", "What-if", FormatCurrency(ScenarioImpact(state, holdings, -0.08)), "Estimated paper portfolio impact if broad market exposure declines 8%."),
            Row("Volatility spike", "What-if", FormatCurrency(ScenarioImpact(state, holdings, -0.05)), "Simplified shock for risk-off volatility expansion."),
            Row("Tech selloff", "What-if", FormatCurrency(ScenarioImpact(state, holdings, -0.04, "tech", -0.12)), "Sector-focused shock applies a deeper decline to technology-labeled holdings."),
            Row("Defensive rotation", "What-if", FormatCurrency(ScenarioImpact(state, holdings, -0.03)), "Broad lower-risk rotation estimate; future version should use factor exposures."),
            Row("Historical comparison", "Planned", "replay windows", "Scenario results should compare against prior drawdown and volatility episodes.", "Review")
        };
    }

    std::vector<InfoItem> BuildResearchCopilotRows(const StockState& state, const std::vector<JournalEntry>& journal)
    {
        return {
            Row("Research chat", "Copilot", "cautious", "Explains symbols, portfolio state, scanner signals, volatility, earnings, and risk without direct buy/sell instructions."),
            Row("Grounding", "Required", std::to_string(journal.size()) + " journal rows", "Responses should cite portfolio state, journal history, prior reports, risk exposure, and data freshness."),
            Row("Forbidden tone", "Locked", "no trading guru", "Avoid guaranteed outcomes, must-buy language, hype, or autonomous live execution suggestions.", "Locked"),
            Row("Selected context", state.selected_symbol.empty() ? "Missing" : "Linked", state.selected_symbol, "Linked panels and chat should share synchronized symbol context.", state.selected_symbol.empty() ? "Review" : "Ready")
        };
    }

    std::vector<InfoItem> BuildLongTermPortfolioIntelligenceRows(const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<JournalEntry>& journal)
    {
        int mistake_count = 0;
        for (const JournalEntry& entry : journal)
        {
            if (!Trim(entry.mistakes).empty())
                ++mistake_count;
        }
        return {
            Row("Quarterly review", "Planned", "portfolio quality", "Generate recurring portfolio quality, drawdown, volatility, and category consistency reports.", "Review"),
            Row("Annual research summary", "Planned", "long horizon", "Summarize strongest research categories, strategy consistency, and risk tolerance over time.", "Review"),
            Row("Behavioral improvement", mistake_count > 0 ? "Tracked" : "Quiet", std::to_string(mistake_count), "Journal mistakes should become long-term coaching inputs without shame or hype."),
            Row("Diversification quality", holdings.size() >= 5 ? "Reviewable" : "Sparse", std::to_string(holdings.size()) + " holdings", "Long-term intelligence improves as portfolio history and holdings depth grow.", holdings.size() >= 5 ? "Ready" : "Review"),
            Row("Market memory", state.loaded_from_api ? "Provider-backed" : "Fallback", state.source_badge, "Historical intelligence should distinguish provider-fresh data from demo/fallback data.", state.loaded_from_api ? "Ready" : "Review")
        };
    }

    std::vector<InfoItem> BuildEnterpriseStabilityRows(const Config& config, const StockState& state)
    {
        return {
            Row("Structured logging", "Active", (AppDataDirectory() / "diagnostics.jsonl").string(), "Provider, rate-limit, cache, smoke-test, and error events write structured diagnostics."),
            Row("Core database", "Active", AuralithCoreDatabasePath().string(), "SQLite WAL database should be checked before migrations, repairs, and report jobs."),
            Row("Task watchdogs", "Foundation", std::to_string(config.refresh_seconds) + " sec refresh", "Background tasks should publish timing, last error, retry, and cancellation state."),
            Row("Safe mode launch", "Planned", "disable providers/plugins", "Safe mode should start with cache/demo data and plugins disabled after repeated crashes.", "Review"),
            Row("Fallback routing", state.loaded_from_api ? "Provider ready" : "Fallback active", state.source_badge, "Provider failures must degrade into labeled cache/demo data instead of anonymous values.", state.loaded_from_api ? "Ready" : "Review"),
            Row("Crash dumps", "Planned", "local-only", "Release builds should capture optional local crash diagnostics with user-controlled telemetry.", "Review")
        };
    }

    std::vector<InfoItem> BuildDeploymentPipelineRows()
    {
        return {
            Row("Installer", "Planned", "MSIX/WiX candidate", "Installer should register app identity, icons, service options, uninstall, and portable mode.", "Review"),
            Row("Release channels", "Planned", "stable/beta/dev", "Channels should support rollback and clear release notes.", "Review"),
            Row("Signed builds", "Required", "future cert", "Production distribution should sign desktop, core, installer, and update packages.", "Review"),
            Row("Asset pipeline", "Foundation", "assets/", "Icons, logos, version resources, and report templates should flow through release scripts."),
            Row("Rollback support", "Planned", "versioned artifacts", "Updater work should wait until installer identity and backup semantics are stable.", "Review")
        };
    }

    std::vector<InfoItem> BuildDocumentationStandardsRows()
    {
        return {
            Row("Architecture docs", "Required", "Core/Desktop/Web/Shared", "Document ownership boundaries before physical repo moves."),
            Row("Engine docs", "Required", "market/signal/risk/portfolio", "Engine contracts should define inputs, outputs, timestamps, and freshness semantics."),
            Row("Provider docs", "Required", "routing/cache/fallback", "Provider adapters must document limits, retry, delay, and fallback behavior."),
            Row("AI behavior docs", "Required", "cautious language", "AI must explain risk and uncertainty without financial advice."),
            Row("Database standards", "Required", "migrations/backups", "SQLite migrations require schema versions, backups, and recovery checks."),
            Row("Audit standards", "Required", "important actions", "Settings, providers, paper execution, reports, plugins, and migrations should be auditable.")
        };
    }

    std::vector<InfoItem> BuildPluginMarketplaceRows()
    {
        return {
            Row("Manifest schema", "Required", "plugins/plugin.schema.json", "Plugins declare id, type, permissions, entrypoints, and compatibility."),
            Row("Indicators", "Extension type", "read-only market data", "Custom indicators should not receive secrets or live execution access."),
            Row("Scanners", "Extension type", "declared data inputs", "Scanners output score, confidence, thesis, and data quality."),
            Row("Providers", "Extension type", "restricted", "Provider adapters require explicit network and secret permissions.", "Review"),
            Row("Reports", "Extension type", "HTML/PDF", "Report plugins should render from shared contracts and audit exports."),
            Row("Sandbox", "Required", "permission-gated", "Marketplace foundations must avoid unrestricted execution and unsafe memory access.", "Review")
        };
    }

    std::vector<InfoItem> BuildCloudSyncPlanningRows()
    {
        return {
            Row("Cloud dependency", "Not required", "local-first", "Auralith must remain usable without cloud sync."),
            Row("Sync scope", "Optional", "settings/watchlists/layouts/reports", "Future sync should be encrypted and user-controlled."),
            Row("AI memory sync", "Sensitive", "opt-in", "AI memory and journal content require careful encryption and export controls.", "Review"),
            Row("Conflict strategy", "Required", "local wins by default", "Multi-device sync needs deterministic conflict resolution before launch.", "Review")
        };
    }

    std::vector<InfoItem> BuildCrossDeviceEcosystemRows()
    {
        return {
            Row("Desktop", "Primary", "research terminal", "Desktop remains the full professional workspace."),
            Row("Web", "Secondary", "dashboard", "Web focuses on monitoring, reports, alerts, paper portfolio review, and mobile-friendly layouts."),
            Row("Mobile", "Companion", "planned", "Mobile should prioritize alerts, reports, risk notifications, briefings, and paper order monitoring.", "Review"),
            Row("Unified state", "Shared", "watchlists/reports/alerts", "Shared contracts and optional sync should prevent divergent device behavior.")
        };
    }

    std::vector<InfoItem> BuildResearchIdentityRows()
    {
        return {
            Row("Tone", "Disciplined", "calm", "Language stays analytical, cautious, and research-driven."),
            Row("Safety", "Research-first", "paper-first", "No hype, no guaranteed outcomes, no accidental live trading paths."),
            Row("Trust", "Transparent", "source/freshness/risk", "Every analysis should expose data quality and uncertainty."),
            Row("Ownership", "Local-first", "user-controlled", "Data, reports, journals, and AI memory belong to the user by default."),
            Row("Positioning", "Professional", "investment intelligence", "Auralith is not a gambling terminal, meme-stock dashboard, or fake hedge fund.")
        };
    }
}
