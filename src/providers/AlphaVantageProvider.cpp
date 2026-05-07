#include "IMarketProvider.h"

namespace aegis
{
    namespace
    {
        class AlphaVantageProvider final : public IMarketProvider
        {
        public:
            std::string Name() const override
            {
                return "Alpha Vantage";
            }

            MarketProviderResponse FetchQuotes(const Config& config, const std::vector<std::string>&)
            {
                MarketProviderResponse response;
                response.provider = Name();
                response.metadata.provider = Name();
                response.metadata.fetched_at = NowTimeLabel();
                response.metadata.freshness = config.alpha_vantage_api_key.empty() ? MarketFreshness::Fallback : MarketFreshness::Delayed;
                response.metadata.live = false;
                response.metadata.fallback = true;
                response.metadata.delayed = true;
                response.metadata.confidence_score = config.alpha_vantage_api_key.empty() ? 42 : 72;
                response.metadata.error_state = config.alpha_vantage_api_key.empty() ? "not-configured" : "routed-through-native-refresh";
                response.metadata.detail = "Native quote refresh still owns the live HTTP call; provider contract is ready for routing.";
                response.error = response.metadata.error_state;
                return response;
            }
        };
    }

    ProviderHealth BuildAlphaVantageProviderHealth(const Config& config)
    {
        ProviderHealth health;
        health.name = "Alpha Vantage";
        health.priority = 10;
        health.enabled = !config.alpha_vantage_api_key.empty();
        health.status = health.enabled ? "Configured" : "Needs key";
        health.health_score = health.enabled ? 78 : 42;
        health.detail = health.enabled
            ? "Primary quote and historical source; routed with cache, TTL, and rate-limit awareness."
            : "No API key configured. Auralith keeps demo/cache fallback clearly labeled.";
        return health;
    }
}
