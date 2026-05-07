#include "IntelligenceMemory.h"

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
            item.tag = "session-memory";
            return item;
        }
    }

    std::vector<InfoItem> BuildSessionMemoryRows(const Config& config, const StockState& state)
    {
        return {
            Row("Session continuity", "Restore", state.selected_symbol.empty() ? "--" : state.selected_symbol, "Active symbol, workspace mode, filters, layout, and chart settings should survive relaunch."),
            Row("Workspace persistence", "Local-first", config.website_base_url, "Desktop and companion should synchronize through AuralithCore local API/IPC."),
            Row("Safety memory", config.paper_only_mode ? "Paper-first" : "Review", config.require_manual_confirmation ? "Manual required" : "Manual off", "Safety settings are persisted and should be audited when changed.", config.paper_only_mode && config.require_manual_confirmation ? "Ready" : "Review"),
            Row("Provider continuity", state.source_badge.empty() ? "Unlabeled" : state.source_badge, state.last_refresh_label.empty() ? "--" : state.last_refresh_label, "Provider freshness and fallback state should be restored from core storage.")
        };
    }
}
