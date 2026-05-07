#include "AuditManager.h"

namespace aegis
{
    namespace
    {
        InfoItem Row(const std::string& name, const std::string& label, const std::string& value, const std::string& detail, const std::string& state)
        {
            InfoItem item;
            item.name = name;
            item.label = label;
            item.value = value;
            item.detail = detail;
            item.state = state;
            item.tag = "compliance";
            return item;
        }
    }

    std::vector<InfoItem> BuildComplianceGuardRows(const Config& config)
    {
        return {
            Row("Paper-first guard", config.paper_only_mode ? "Enabled" : "Review", config.paper_only_mode ? "ON" : "OFF", "Execution-like workflows should stay in paper simulation unless future code explicitly unlocks live trading.", config.paper_only_mode ? "Ready" : "Review"),
            Row("Manual confirmation", config.require_manual_confirmation ? "Required" : "Review", config.require_manual_confirmation ? "Required" : "Not required", "Paper execution attempts should require a deliberate confirmation step.", config.require_manual_confirmation ? "Ready" : "Review"),
            Row("Advice boundary", "Required", "Research only", "Auralith can explain conditions, risk, and scenarios; it must not present outputs as financial advice.", "Ready"),
            Row("Provider transparency", "Required", "source + freshness", "No quote, scanner result, chart bundle, or report should render without provider metadata.", "Ready")
        };
    }
}
