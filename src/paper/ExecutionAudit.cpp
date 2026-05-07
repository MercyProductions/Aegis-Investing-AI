#include "PaperBroker.h"

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
            item.tag = "paper-audit";
            return item;
        }
    }

    std::vector<InfoItem> BuildExecutionAuditRows(const PaperOrderResult& result)
    {
        return {
            Row("Order state", PaperOrderStateName(result.state), result.accepted ? "Accepted" : "Blocked", result.detail, result.accepted ? "Ready" : "Review"),
            Row("Fill quantity", result.filled_shares > 0.0 ? "Simulated" : "None", std::to_string(static_cast<int>(result.filled_shares)) + " / " + std::to_string(static_cast<int>(result.requested_shares)), "Paper execution records requested and filled share counts.", result.filled_shares > 0.0 ? "Ready" : "Review"),
            Row("Average fill", "Paper", result.average_fill_price > 0.0 ? FormatCurrency(result.average_fill_price) : "--", "Average simulated fill price after slippage."),
            Row("Estimated cost", "Paper", FormatCurrency(result.commission), "Commission model is explicit and visible for future broker abstraction."),
            Row("Audit event", result.audit_action.empty() ? "Not written" : "Prepared", result.audit_action, "Every attempted execution should be recorded by the audit layer.", result.audit_action.empty() ? "Review" : "Ready")
        };
    }
}
