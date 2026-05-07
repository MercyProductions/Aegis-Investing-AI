#include "IMarketProvider.h"

#include "../Platform.h"

namespace aegis
{
    ProviderHealth BuildFinnhubProviderHealth()
    {
        ProviderHealth health;
        health.name = "Finnhub";
        health.priority = 20;
        health.enabled = !GetEnvUtf8(L"AURALITH_FINNHUB_API_KEY").empty();
        health.status = health.enabled ? "Available" : "Standby";
        health.health_score = health.enabled ? 70 : 35;
        health.detail = health.enabled
            ? "Environment key detected; intended for future news, earnings, and quote redundancy."
            : "Optional provider placeholder. No anonymous Finnhub data is used until the key and route are explicit.";
        return health;
    }
}
