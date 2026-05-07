#pragma once

#include "../StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    enum class MarketFreshness
    {
        Live,
        Delayed,
        Stale,
        Fallback,
        Error
    };

    struct MarketDataMetadata
    {
        std::string provider;
        std::string fetched_at;
        MarketFreshness freshness = MarketFreshness::Fallback;
        bool live = false;
        bool fallback = true;
        bool delayed = true;
        bool rate_limited = false;
        int confidence_score = 50;
        std::string error_state;
        std::string detail;
    };

    struct ProviderHealth
    {
        std::string name;
        std::string status;
        std::string detail;
        int priority = 0;
        int health_score = 0;
        bool enabled = false;
        bool rate_limited = false;
    };

    struct MarketProviderResponse
    {
        bool ok = false;
        std::string provider;
        MarketDataMetadata metadata;
        std::vector<StockQuote> quotes;
        std::string error;
    };

    class IMarketProvider
    {
    public:
        virtual ~IMarketProvider() = default;
        virtual std::string Name() const = 0;
        virtual MarketProviderResponse FetchQuotes(const Config& config, const std::vector<std::string>& symbols) = 0;
    };

    const char* MarketFreshnessName(MarketFreshness freshness);
    ProviderHealth BuildAlphaVantageProviderHealth(const Config& config);
    ProviderHealth BuildFinnhubProviderHealth();
    ProviderHealth BuildPolygonProviderHealth();
    MarketProviderResponse BuildDemoProviderResponse(const Config& config);
    MarketProviderResponse BuildCachedProviderResponse(const StockState& state, const std::string& reason);
    std::vector<InfoItem> BuildProviderPipelineRows(const Config& config, const StockState& state);
}
