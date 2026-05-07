#include "MarketEngine.h"

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
            item.tag = "market-engine";
            return item;
        }

        bool IsFallbackQuote(const StockQuote& quote)
        {
            const std::string combined = Lower(quote.source + " " + quote.status + " " + quote.note + " " + quote.data_quality);
            return quote.fallback || !quote.live || combined.find("demo") != std::string::npos || combined.find("fallback") != std::string::npos || combined.find("simulated") != std::string::npos;
        }

        int ClampScore(int value)
        {
            return std::max(0, std::min(100, value));
        }
    }

    MarketEngineSnapshot BuildMarketEngineSnapshot(const Config& config, const StockState& state)
    {
        MarketEngineSnapshot snapshot;
        snapshot.session_state = state.market_status.empty() ? "Unknown" : state.market_status;
        snapshot.source_label = state.source_badge.empty() ? "Unlabeled provider" : state.source_badge;
        snapshot.freshness_label = state.last_refresh_label.empty() ? "--" : state.last_refresh_label;
        snapshot.symbols = static_cast<int>(state.quotes.size());

        int confidence_total = 0;
        for (const StockQuote& quote : state.quotes)
        {
            if (quote.live)
                ++snapshot.live_symbols;
            if (IsFallbackQuote(quote))
                ++snapshot.fallback_symbols;
            const std::string quality = Lower(quote.data_quality + " " + quote.status + " " + quote.note);
            if (quote.delayed || quality.find("stale") != std::string::npos || quality.find("delayed") != std::string::npos)
                ++snapshot.stale_symbols;
            confidence_total += ClampScore(quote.data_confidence > 0 ? quote.data_confidence : 100 - DataConfidencePenalty(quote));
        }
        snapshot.average_confidence = snapshot.symbols > 0 ? ClampScore(confidence_total / snapshot.symbols) : 0;

        snapshot.rows = {
            Row("Provider orchestration", snapshot.source_label, std::to_string(snapshot.symbols) + " symbols", state.source_label.empty() ? "Provider source is labeled at quote and workspace level." : state.source_label),
            Row("Refresh schedule", "Configured", std::to_string(config.refresh_seconds) + " sec", "Global refresh cadence is centralized for the market engine boundary."),
            Row("Freshness state", snapshot.stale_symbols == 0 ? "Fresh/known" : "Review", snapshot.freshness_label, "Every quote should carry provider, timestamp, freshness, and fallback state.", snapshot.stale_symbols == 0 ? "Ready" : "Review"),
            Row("Research confidence", snapshot.average_confidence >= 75 ? "High" : (snapshot.average_confidence >= 50 ? "Medium" : "Low"), std::to_string(snapshot.average_confidence) + "/100", "Average confidence from provider source, freshness, fallback state, and estimated fundamentals.", snapshot.average_confidence >= 75 ? "Ready" : "Review"),
            Row("Source mix", snapshot.fallback_symbols == 0 ? "Provider-backed" : "Mixed", std::to_string(snapshot.live_symbols) + " live / " + std::to_string(snapshot.fallback_symbols) + " fallback", "Fallback and demo rows stay visible so no anonymous data reaches the UI.", snapshot.fallback_symbols == 0 ? "Ready" : "Review"),
            Row("Market session", snapshot.session_state, state.tier, state.market_detail.empty() ? "Market session state has not been refreshed." : state.market_detail, snapshot.session_state == "Unknown" ? "Review" : "Ready")
        };

        return snapshot;
    }

    std::vector<InfoItem> BuildMarketEngineRows(const Config& config, const StockState& state)
    {
        return BuildMarketEngineSnapshot(config, state).rows;
    }
}
