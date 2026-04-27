#include "WorkflowValidation.h"

#include "Platform.h"
#include "SymbolRules.h"

#include <cmath>

namespace aegis
{
    namespace
    {
        constexpr size_t kMaxShortNoteLength = 160;
        constexpr size_t kMaxLongTextLength = 1024;
        constexpr double kAlertPriceTolerance = 0.0001;
    }

    HoldingDraftResult ValidateHoldingDraft(const std::string& symbol, double shares, double average_cost, const std::string& note)
    {
        HoldingDraftResult result;
        const SymbolValidationResult validated_symbol = ValidateTickerSymbol(symbol);
        if (!validated_symbol.ok)
        {
            result.error = validated_symbol.reason;
            return result;
        }
        if (shares <= 0.0)
        {
            result.error = "Shares must be positive.";
            return result;
        }
        if (average_cost < 0.0)
        {
            result.error = "Average cost must be zero or positive.";
            return result;
        }
        const std::string trimmed_note = Trim(note);
        if (trimmed_note.size() > kMaxShortNoteLength)
        {
            result.error = "Note is too long for the holding editor.";
            return result;
        }

        result.row.symbol = validated_symbol.symbol;
        result.row.shares = shares;
        result.row.average_cost = average_cost;
        result.row.note = trimmed_note;
        result.ok = true;
        return result;
    }

    AlertDraftResult ValidateAlertDraft(const std::string& symbol, double trigger_price, bool above, bool enabled, const std::string& note)
    {
        AlertDraftResult result;
        const SymbolValidationResult validated_symbol = ValidateTickerSymbol(symbol);
        if (!validated_symbol.ok)
        {
            result.error = validated_symbol.reason;
            return result;
        }
        if (trigger_price <= 0.0)
        {
            result.error = "Trigger price must be positive.";
            return result;
        }
        const std::string trimmed_note = Trim(note);
        if (trimmed_note.size() > kMaxShortNoteLength)
        {
            result.error = "Note is too long for the alert editor.";
            return result;
        }

        result.row.symbol = validated_symbol.symbol;
        result.row.trigger_price = trigger_price;
        result.row.above = above;
        result.row.enabled = enabled;
        result.row.note = trimmed_note;
        result.ok = true;
        return result;
    }

    TradePlanDraftResult ValidateTradePlanDraft(const TradePlan& plan)
    {
        TradePlanDraftResult result;
        const SymbolValidationResult validated_symbol = ValidateTickerSymbol(plan.symbol);
        if (!validated_symbol.ok)
        {
            result.error = validated_symbol.reason;
            return result;
        }
        if (plan.entry <= 0.0)
        {
            result.error = "Entry must be positive.";
            return result;
        }
        if (plan.stop <= 0.0)
        {
            result.error = "Stop must be positive.";
            return result;
        }
        if (plan.target <= 0.0)
        {
            result.error = "Target must be positive.";
            return result;
        }
        if (plan.stop >= plan.entry)
        {
            result.error = "Stop must be below entry for long paper plans.";
            return result;
        }
        if (plan.target <= plan.entry)
        {
            result.error = "Target must be above entry for long paper plans.";
            return result;
        }
        if (plan.shares <= 0)
        {
            result.error = "Shares must be positive.";
            return result;
        }
        if (plan.planned_risk <= 0.0)
        {
            result.error = "Planned risk must be positive.";
            return result;
        }
        if (plan.planned_reward <= 0.0)
        {
            result.error = "Planned reward must be positive.";
            return result;
        }
        if (!std::isfinite(plan.entry) || !std::isfinite(plan.stop) || !std::isfinite(plan.target) ||
            !std::isfinite(plan.planned_risk) || !std::isfinite(plan.planned_reward))
        {
            result.error = "Plan prices and risk/reward must be finite numbers.";
            return result;
        }
        if (Trim(plan.thesis).size() > kMaxLongTextLength)
        {
            result.error = "Plan thesis is too long.";
            return result;
        }

        result.row = plan;
        result.row.symbol = validated_symbol.symbol;
        result.row.created_at = Trim(result.row.created_at).empty() ? NowTimeLabel() : Trim(result.row.created_at);
        result.row.rating = Trim(result.row.rating).empty() ? "Watch" : Trim(result.row.rating);
        result.row.status = Trim(result.row.status).empty() ? "Open" : Trim(result.row.status);
        result.row.thesis = Trim(result.row.thesis);
        result.ok = true;
        return result;
    }

    JournalDraftResult ValidateJournalDraft(const std::string& time,
        const std::string& symbol,
        const std::string& action,
        const std::string& reason,
        const std::string& exit_reason,
        const std::string& tags,
        const std::string& mistakes,
        const std::string& grade,
        double realized_pnl)
    {
        JournalDraftResult result;
        const SymbolValidationResult validated_symbol = ValidateTickerSymbol(symbol);
        if (!validated_symbol.ok)
        {
            result.error = validated_symbol.reason;
            return result;
        }
        if (Trim(action).empty())
        {
            result.error = "Action cannot be empty.";
            return result;
        }
        if (!std::isfinite(realized_pnl))
        {
            result.error = "Realized P/L must be finite.";
            return result;
        }
        if (Trim(reason).size() > kMaxLongTextLength || Trim(exit_reason).size() > kMaxLongTextLength ||
            Trim(tags).size() > kMaxShortNoteLength || Trim(mistakes).size() > kMaxLongTextLength ||
            Trim(grade).size() > 32)
        {
            result.error = "Journal text is too long.";
            return result;
        }

        result.row.time = Trim(time).empty() ? NowTimeLabel() : Trim(time);
        result.row.symbol = validated_symbol.symbol;
        result.row.action = Trim(action);
        result.row.reason = Trim(reason);
        result.row.exit_reason = Trim(exit_reason);
        result.row.tags = Trim(tags);
        result.row.mistakes = Trim(mistakes);
        result.row.grade = Trim(grade);
        result.row.realized_pnl = realized_pnl;
        result.ok = true;
        return result;
    }

    int FindDuplicateHoldingIndex(const std::vector<PortfolioHolding>& holdings, const std::string& symbol, int ignore_index)
    {
        const std::string target = NormalizeTickerSymbol(symbol);
        if (target.empty())
            return -1;
        for (int i = 0; i < static_cast<int>(holdings.size()); ++i)
        {
            if (i == ignore_index)
                continue;
            if (Upper(holdings[static_cast<size_t>(i)].symbol) == target)
                return i;
        }
        return -1;
    }

    int FindDuplicateAlertIndex(const std::vector<PriceAlert>& alerts, const PriceAlert& alert, int ignore_index)
    {
        for (int i = 0; i < static_cast<int>(alerts.size()); ++i)
        {
            if (i == ignore_index)
                continue;
            const PriceAlert& row = alerts[static_cast<size_t>(i)];
            if (Upper(row.symbol) == Upper(alert.symbol) &&
                row.above == alert.above &&
                std::fabs(row.trigger_price - alert.trigger_price) <= kAlertPriceTolerance)
            {
                return i;
            }
        }
        return -1;
    }
}
