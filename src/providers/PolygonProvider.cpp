#include "IMarketProvider.h"

#include "../Platform.h"

namespace aegis
{
    ProviderHealth BuildPolygonProviderHealth()
    {
        ProviderHealth health;
        health.name = "Polygon";
        health.priority = 30;
        health.enabled = !GetEnvUtf8(L"AURALITH_POLYGON_API_KEY").empty();
        health.status = health.enabled ? "Available" : "Standby";
        health.health_score = health.enabled ? 72 : 35;
        health.detail = health.enabled
            ? "Environment key detected; intended for deeper market data and future multi-asset routing."
            : "Optional provider placeholder. Data remains cache/demo labeled until Polygon routing is intentionally wired.";
        return health;
    }
}
