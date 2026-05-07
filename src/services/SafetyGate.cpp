#include "SafetyGate.h"

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
            item.tag = "safety";
            return item;
        }
    }

    SafetyGateResult EvaluateSafetyGate(const Config& config, const SafetyGateInput& input)
    {
        if (input.intent == ExecutionIntent::LiveExecution)
        {
            return {
                false,
                "Blocked",
                "Live execution is locked. Auralith permits research workflows and paper execution simulation only.",
                "execution.live_blocked"
            };
        }

        if (!config.paper_only_mode)
        {
            return {
                false,
                "Review",
                "Paper-only mode must remain enabled before any execution-like workflow can continue.",
                "execution.paper_mode_required"
            };
        }

        if (input.intent == ExecutionIntent::PaperExecution && config.require_manual_confirmation && !input.manual_confirmed)
        {
            return {
                false,
                "Confirmation required",
                "Manual confirmation is required before paper execution simulation.",
                "execution.manual_confirmation_required"
            };
        }

        return {
            true,
            "Guarded",
            input.intent == ExecutionIntent::PaperExecution
                ? "Paper execution simulation is allowed after manual review."
                : "Research simulation is allowed with paper-only guardrails active.",
            "execution.paper_allowed"
        };
    }

    std::string CurrentRiskStateLabel(const Config& config)
    {
        if (config.paper_only_mode && config.require_manual_confirmation)
            return "Guarded";
        if (config.paper_only_mode)
            return "Paper review";
        return "Needs review";
    }

    std::vector<InfoItem> BuildSafetyGateRows(const Config& config)
    {
        const SafetyGateResult paper_gate = EvaluateSafetyGate(config, { ExecutionIntent::PaperExecution, true, "" });
        const SafetyGateResult live_gate = EvaluateSafetyGate(config, { ExecutionIntent::LiveExecution, false, "" });

        return {
            Row("Paper mode", config.paper_only_mode ? "ON" : "Review", config.paper_only_mode ? "Locked" : "Off", "Auralith is designed as a research-first, paper-first market cockpit.", config.paper_only_mode ? "Ready" : "Review"),
            Row("Manual confirmation", config.require_manual_confirmation ? "Required" : "Review", config.require_manual_confirmation ? "Required" : "Not required", "Paper execution simulations should require an explicit user confirmation step.", config.require_manual_confirmation ? "Ready" : "Review"),
            Row("Paper execution gate", paper_gate.allowed ? "Allowed" : "Blocked", paper_gate.state, paper_gate.reason, paper_gate.allowed ? "Ready" : "Review"),
            Row("Live execution gate", live_gate.allowed ? "Allowed" : "Blocked", live_gate.state, live_gate.reason, "Locked"),
            Row("Financial advice boundary", "Visible", "Research only", "Auralith surfaces signals, risk context, and paper plans; it must not imply financial advice or guaranteed outcomes.", "Ready")
        };
    }
}
