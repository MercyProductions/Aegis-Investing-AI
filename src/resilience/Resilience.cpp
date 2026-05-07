#include "Resilience.h"

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
            item.tag = "resilience";
            return item;
        }
    }

    std::vector<InfoItem> BuildResilienceRows(const Config& config, const StockState& state)
    {
        const std::string combined = Lower(state.source_badge + " " + state.source_label + " " + state.market_detail + " " + state.last_refresh_label);
        const bool fallback = combined.find("fallback") != std::string::npos || combined.find("demo") != std::string::npos || !state.loaded_from_api;
        const bool stale = combined.find("stale") != std::string::npos || combined.find("delayed") != std::string::npos;
        return {
            Row("Provider outage handling", fallback ? "Fallback active" : "Provider ready", state.source_badge, "Core service should continue with labeled cache/demo fallback during provider outages.", fallback ? "Review" : "Ready"),
            Row("Stale-data validation", stale ? "Review" : "Clear", state.last_refresh_label.empty() ? "--" : state.last_refresh_label, "Stale data should reduce analysis confidence and create notifications.", stale ? "Review" : "Ready"),
            Row("Internet disconnects", "Graceful", "Cache-first", "HistoricalStore and CoreDatabase should support offline charts/reports from local storage."),
            Row("Corrupted cache repair", "Planned", std::to_string(config.max_cache_mb) + " MB budget", "Core repair routines should validate and prune corrupt cache/database artifacts."),
            Row("Long-running sessions", "Required", std::to_string(config.refresh_seconds) + " sec refresh", "Background jobs should record timing, failures, and recovery status without blocking the UI.")
        };
    }

    std::vector<InfoItem> BuildRecoveryPlanRows()
    {
        return {
            Row("Recovery logs", "Required", "diagnostics.jsonl", "Provider failures, partial syncs, database repairs, and crash diagnostics should write structured events."),
            Row("Automatic backups", "Required", "snapshots", "CoreDatabase backup snapshots should run before migrations and on scheduled intervals."),
            Row("Database recovery", "Planned", "integrity_check", "Core should run SQLite integrity checks and attempt repair/restore when corruption is detected."),
            Row("Partial sync recovery", "Required", "idempotent jobs", "Background jobs should be retryable without duplicating audit or timeline events.")
        };
    }
}
