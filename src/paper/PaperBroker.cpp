#include "PaperBroker.h"

#include "../services/SafetyGate.h"

namespace aegis
{
    const char* PaperOrderStateName(PaperOrderState state)
    {
        switch (state)
        {
        case PaperOrderState::Pending: return "Pending";
        case PaperOrderState::Queued: return "Queued";
        case PaperOrderState::PartiallyFilled: return "Partially filled";
        case PaperOrderState::Filled: return "Filled";
        case PaperOrderState::Cancelled: return "Cancelled";
        case PaperOrderState::Rejected: return "Rejected";
        case PaperOrderState::Expired: return "Expired";
        default: return "Unknown";
        }
    }

    PaperOrderResult SubmitPaperOrder(const Config& config, const StockQuote& quote, const PaperOrderRequest& request)
    {
        PaperOrderResult result;
        result.requested_shares = request.shares;

        const SafetyGateResult gate = EvaluateSafetyGate(config, {
            request.force_live ? ExecutionIntent::LiveExecution : ExecutionIntent::PaperExecution,
            request.manual_confirmed,
            request.symbol.empty() ? quote.symbol : request.symbol
        });

        if (!gate.allowed)
        {
            result.blocked = true;
            result.state = gate.state == "Confirmation required" ? PaperOrderState::Queued : PaperOrderState::Rejected;
            result.audit_action = gate.audit_action;
            result.detail = gate.reason;
            result.rows = BuildExecutionAuditRows(result);
            return result;
        }

        if (request.shares <= 0.0 || quote.price <= 0.0)
        {
            result.blocked = true;
            result.state = PaperOrderState::Rejected;
            result.audit_action = "paper_order.rejected_invalid_input";
            result.detail = "Paper order rejected because shares or reference price were not usable.";
            result.rows = BuildExecutionAuditRows(result);
            return result;
        }

        const PaperFill fill = SimulatePaperFill(quote, request);
        result.accepted = true;
        result.state = fill.shares < request.shares ? PaperOrderState::PartiallyFilled : PaperOrderState::Filled;
        result.filled_shares = fill.shares;
        result.average_fill_price = fill.price;
        result.notional = fill.price * fill.shares;
        result.commission = fill.commission;
        result.latency_ms = fill.latency_ms;
        result.audit_action = result.state == PaperOrderState::Filled ? "paper_order.filled" : "paper_order.partially_filled";
        result.detail = fill.detail;
        result.rows = BuildExecutionAuditRows(result);
        return result;
    }
}
