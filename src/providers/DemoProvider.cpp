#include "IMarketProvider.h"

namespace aegis
{
    MarketProviderResponse BuildDemoProviderResponse(const Config&)
    {
        const StockState demo = MakeDemoStockState();
        MarketProviderResponse response;
        response.ok = true;
        response.provider = "Demo Provider";
        response.quotes = demo.quotes;
        response.metadata.provider = "Demo Provider";
        response.metadata.fetched_at = NowTimeLabel();
        response.metadata.freshness = MarketFreshness::Fallback;
        response.metadata.live = false;
        response.metadata.fallback = true;
        response.metadata.delayed = true;
        response.metadata.confidence_score = 55;
        response.metadata.detail = "Built-in demonstration data for offline research workflows.";
        return response;
    }
}
