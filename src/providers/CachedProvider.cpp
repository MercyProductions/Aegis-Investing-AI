#include "IMarketProvider.h"

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
            item.tag = "provider-pipeline";
            return item;
        }

        InfoItem HealthRow(const ProviderHealth& health)
        {
            return Row(
                health.name,
                health.status,
                std::to_string(health.health_score) + "/100",
                "Priority " + std::to_string(health.priority) + ". " + health.detail,
                health.enabled ? "Ready" : "Standby");
        }

        bool LooksFallback(const StockQuote& quote)
        {
            const std::string text = Lower(quote.source + " " + quote.status + " " + quote.note + " " + quote.data_quality);
            return quote.fallback || !quote.live || text.find("demo") != std::string::npos || text.find("fallback") != std::string::npos || text.find("synthetic") != std::string::npos;
        }

        int AverageConfidence(const std::vector<StockQuote>& quotes)
        {
            if (quotes.empty())
                return 0;
            int total = 0;
            for (const StockQuote& quote : quotes)
            {
                const int explicit_score = quote.data_confidence > 0 ? quote.data_confidence : 100 - DataConfidencePenalty(quote);
                total += std::max(0, std::min(100, explicit_score));
            }
            return std::max(0, std::min(100, total / static_cast<int>(quotes.size())));
        }
    }

    const char* MarketFreshnessName(MarketFreshness freshness)
    {
        switch (freshness)
        {
        case MarketFreshness::Live: return "Live";
        case MarketFreshness::Delayed: return "Delayed";
        case MarketFreshness::Stale: return "Stale";
        case MarketFreshness::Fallback: return "Fallback";
        case MarketFreshness::Error: return "Error";
        default: return "Unknown";
        }
    }

    MarketProviderResponse BuildCachedProviderResponse(const StockState& state, const std::string& reason)
    {
        MarketProviderResponse response;
        response.ok = !state.quotes.empty();
        response.provider = state.source_badge.empty() ? "Cached Provider" : state.source_badge;
        response.quotes = state.quotes;
        response.metadata.provider = response.provider;
        response.metadata.fetched_at = state.last_refresh_label.empty() ? NowTimeLabel() : state.last_refresh_label;
        response.metadata.live = state.loaded_from_api;
        response.metadata.fallback = false;
        response.metadata.delayed = false;
        response.metadata.freshness = state.loaded_from_api ? MarketFreshness::Live : MarketFreshness::Fallback;
        response.metadata.confidence_score = AverageConfidence(state.quotes);
        response.metadata.detail = reason.empty() ? "Current in-memory quote board." : reason;

        for (const StockQuote& quote : state.quotes)
        {
            if (LooksFallback(quote))
                response.metadata.fallback = true;
            const std::string quality = Lower(quote.data_quality + " " + quote.status + " " + quote.note);
            if (quote.delayed || quality.find("stale") != std::string::npos || quality.find("delayed") != std::string::npos)
                response.metadata.delayed = true;
        }

        if (response.metadata.fallback)
            response.metadata.freshness = MarketFreshness::Fallback;
        else if (response.metadata.delayed)
            response.metadata.freshness = MarketFreshness::Delayed;
        return response;
    }

    std::vector<InfoItem> BuildProviderPipelineRows(const Config& config, const StockState& state)
    {
        std::vector<InfoItem> rows;
        const ProviderHealth alpha = BuildAlphaVantageProviderHealth(config);
        const ProviderHealth finnhub = BuildFinnhubProviderHealth();
        const ProviderHealth polygon = BuildPolygonProviderHealth();
        rows.push_back(HealthRow(alpha));
        rows.push_back(HealthRow(finnhub));
        rows.push_back(HealthRow(polygon));

        const MarketProviderResponse cached = BuildCachedProviderResponse(state, "Current quote board used by UI, scanners, risk, and reports.");
        rows.push_back(Row("Cached provider", MarketFreshnessName(cached.metadata.freshness), std::to_string(static_cast<int>(cached.quotes.size())) + " quotes", cached.metadata.detail + " Confidence " + std::to_string(cached.metadata.confidence_score) + "/100.", cached.metadata.fallback ? "Review" : "Ready"));

        const MarketProviderResponse demo = BuildDemoProviderResponse(config);
        rows.push_back(Row("Demo provider", MarketFreshnessName(demo.metadata.freshness), std::to_string(static_cast<int>(demo.quotes.size())) + " quotes", demo.metadata.detail, "Standby"));

        const bool primary_live = alpha.enabled && state.loaded_from_api;
        rows.push_back(Row("Priority routing", primary_live ? "Primary live" : "Fallback visible", primary_live ? alpha.name : cached.provider, "Provider priority routes through cache first when fresh, then configured providers, then clearly tagged demo fallback.", primary_live ? "Ready" : "Review"));
        rows.push_back(Row("Freshness contract", "Required", "provider + fetchedAt", "Every quote, chart bundle, report, scanner signal, and research panel should expose source metadata before rendering."));
        rows.push_back(Row("Rate-limit handling", alpha.enabled ? "Configured" : "Passive", std::to_string(config.alpha_quote_ttl_seconds) + " sec TTL", "Backoff and cache fallback are centralized at the provider boundary so UI code does not guess."));
        return rows;
    }
}
