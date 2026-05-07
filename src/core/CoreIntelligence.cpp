#include "CoreIntelligence.h"

#include "../engine/MarketEngine.h"
#include "../engine/SignalEngine.h"
#include "../Platform.h"

#include <algorithm>

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
            item.tag = "auralith-core";
            item.source = "AuralithCore";
            item.time = NowTimeLabel();
            return item;
        }

        void AppendRows(std::vector<InfoItem>& target, const std::vector<InfoItem>& source)
        {
            target.insert(target.end(), source.begin(), source.end());
        }

        int ClampConfidence(int confidence)
        {
            return std::max(0, std::min(100, confidence));
        }
    }

    CoreProviderSnapshot BuildCoreProviderSnapshot(const Config& config, const StockState& state)
    {
        CoreProviderSnapshot snapshot;
        snapshot.response = BuildCachedProviderResponse(state, "AuralithCore provider snapshot for shared desktop, web, and future companion clients.");

        if (snapshot.response.quotes.empty())
            snapshot.response = BuildDemoProviderResponse(config);

        snapshot.source = snapshot.response.metadata.provider.empty() ? snapshot.response.provider : snapshot.response.metadata.provider;
        snapshot.freshness = MarketFreshnessName(snapshot.response.metadata.freshness);
        snapshot.fetched_at = snapshot.response.metadata.fetched_at;
        snapshot.quote_count = static_cast<int>(snapshot.response.quotes.size());
        snapshot.confidence = ClampConfidence(snapshot.response.metadata.confidence_score);
        snapshot.live = snapshot.response.metadata.live;
        snapshot.fallback = snapshot.response.metadata.fallback;
        snapshot.delayed = snapshot.response.metadata.delayed;

        snapshot.rows.push_back(Row(
            "Core ownership",
            "Provider layer",
            "AuralithCore",
            "Provider normalization, routing, freshness, fallback labeling, and confidence scoring are now surfaced as a Core-owned shared intelligence boundary."));
        snapshot.rows.push_back(Row(
            "Provider snapshot",
            snapshot.freshness,
            snapshot.source,
            std::to_string(snapshot.quote_count) + " symbols, confidence " + std::to_string(snapshot.confidence) + "/100, fetched " + snapshot.fetched_at + ".",
            snapshot.fallback || snapshot.delayed ? "Review" : "Ready"));
        snapshot.rows.push_back(Row(
            "Client contract",
            "Shared Core",
            "desktop + web + mobile",
            "Clients should render this provider snapshot instead of reconstructing provider state independently."));
        snapshot.rows.push_back(Row(
            "Routing order",
            "Centralized",
            "cache -> providers -> demo",
            "The desktop terminal remains the advanced workstation, but provider decisions should move through Core before reaching any UI."));

        AppendRows(snapshot.rows, BuildProviderPipelineRows(config, state));

        const MarketEngineSnapshot market = BuildMarketEngineSnapshot(config, state);
        snapshot.rows.push_back(Row(
            "MarketEngine handoff",
            market.freshness_label,
            market.source_label,
            "MarketEngine consumes the provider snapshot and publishes session, source, and confidence rows for all clients.",
            market.fallback_symbols > 0 || market.stale_symbols > 0 ? "Review" : "Ready"));

        return snapshot;
    }

    std::vector<InfoItem> BuildCoreExtractionRoadmapRows()
    {
        return {
            Row("1. Provider layer", "In progress", "Core-owned", "Normalize providers, cache/fallback routing, source metadata, and freshness confidence in AuralithCore.", "Active"),
            Row("2. Scanner engine", "Next", "Centralize", "Move signal generation, momentum setup logic, conviction, and expected sell zones behind the shared Core boundary.", "Queued"),
            Row("3. Alert engine", "Next", "Centralize", "Run alert evaluation and notification dispatch through Core so desktop and web share the same trigger history.", "Queued"),
            Row("4. Portfolio engine", "Planned", "Centralize", "Move cost basis, paper portfolio state, allocation, exposure, and risk handoff into Core.", "Planned"),
            Row("5. Report engine", "Planned", "Centralize", "Generate morning, risk, portfolio, and strategy reports from Core-owned data and templates.", "Planned"),
            Row("6. AI systems", "Planned", "Centralize", "Run summaries, narratives, memory, and portfolio reviews from shared context with cautious language.", "Planned"),
            Row("7. Historical storage", "Planned", "Centralize", "Move candles, indicators, retention, validation, and missing-data recovery into the Core database layer.", "Planned")
        };
    }

    std::vector<InfoItem> BuildCoreSharedIntelligenceRows(const Config& config, const StockState& state)
    {
        std::vector<InfoItem> rows;
        const CoreProviderSnapshot provider = BuildCoreProviderSnapshot(config, state);
        AppendRows(rows, provider.rows);

        const SignalEngineSnapshot signals = BuildSignalEngineSnapshot(state);
        rows.push_back(Row(
            "Scanner extraction",
            signals.signal_count > 0 ? "Ready to promote" : "Waiting",
            std::to_string(signals.signal_count) + " signals",
            "Signal generation should be the next shared intelligence subsystem after provider ownership stabilizes.",
            signals.signal_count > 0 ? "Queued" : "Review"));

        AppendRows(rows, BuildCoreExtractionRoadmapRows());
        return rows;
    }
}
