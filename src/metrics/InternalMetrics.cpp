#include "InternalMetrics.h"

#include "../Platform.h"

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
            item.tag = "internal-metrics";
            return item;
        }
    }

    std::vector<InfoItem> BuildInternalMetricsRows(const Config& config, const StockState& state)
    {
        int fallback_quotes = 0;
        for (const StockQuote& quote : state.quotes)
        {
            const std::string quality = Lower(quote.source + " " + quote.status + " " + quote.note + " " + quote.data_quality);
            if (!quote.live || quality.find("fallback") != std::string::npos || quality.find("demo") != std::string::npos)
                ++fallback_quotes;
        }
        return {
            Row("Provider reliability", fallback_quotes == 0 ? "Provider-backed" : "Mixed", std::to_string(static_cast<int>(state.quotes.size()) - fallback_quotes) + " live", "Tracks provider success, fallback usage, and freshness."),
            Row("Scanner performance", "Tracked", std::to_string(static_cast<int>(state.signals.size())), "scanner_history will make signal outcome analysis possible."),
            Row("Strategy performance", "Tracked", std::to_string(config.model_count), "Backtest and paper outcomes should feed strategy quality metrics."),
            Row("Alert usefulness", "Planned", "ack/snooze", "Alert events should track acknowledgements, snoozes, false positives, and usefulness."),
            Row("Data freshness", fallback_quotes > 0 ? "Review" : "Ready", state.last_refresh_label.empty() ? "--" : state.last_refresh_label, "Freshness metrics should influence every analysis confidence label.", fallback_quotes > 0 ? "Review" : "Ready"),
            Row("Task timing", "Planned", std::to_string(config.refresh_seconds) + " sec", "Core task scheduler should record duration, failures, retries, and last success."),
            Row("Memory usage", "Planned", "process metrics", "Long-running service should record memory and CPU trends for operational maturity.")
        };
    }
}
