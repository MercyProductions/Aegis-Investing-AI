#include "ImportValidation.h"

#include "Platform.h"
#include "SymbolRules.h"

#include <cstdlib>

namespace aegis
{
    namespace
    {
        bool ParseDoubleStrict(const std::string& value, double& out)
        {
            const std::string trimmed = Trim(value);
            if (trimmed.empty())
                return false;
            char* end = nullptr;
            out = std::strtod(trimmed.c_str(), &end);
            return end != trimmed.c_str() && Trim(end ? std::string(end) : std::string{}).empty();
        }

        bool ParseIntStrict(const std::string& value, int& out)
        {
            const std::string trimmed = Trim(value);
            if (trimmed.empty())
                return false;
            char* end = nullptr;
            const long parsed = std::strtol(trimmed.c_str(), &end, 10);
            if (end == trimmed.c_str() || !Trim(end ? std::string(end) : std::string{}).empty())
                return false;
            out = static_cast<int>(parsed);
            return true;
        }

        bool ParseEnabled(const std::string& value)
        {
            const std::string normalized = Lower(Trim(value));
            return !(normalized == "false" || normalized == "0" || normalized == "no" || normalized == "disabled");
        }

        bool ParseAbove(const std::string& value)
        {
            const std::string normalized = Lower(Trim(value));
            return !(normalized == "below" || normalized == "under" || normalized == "<");
        }
    }

    HoldingImportResult ParseHoldingImportRow(const std::vector<std::string>& cells)
    {
        HoldingImportResult result;
        if (cells.size() < 3)
        {
            result.error = "Expected symbol, shares, and average cost.";
            return result;
        }
        const SymbolValidationResult symbol = ValidateTickerSymbol(cells[0]);
        if (!symbol.ok)
        {
            result.error = symbol.reason;
            return result;
        }
        double shares = 0.0;
        double average_cost = 0.0;
        if (!ParseDoubleStrict(cells[1], shares) || shares <= 0.0)
        {
            result.error = "Shares must be a positive number.";
            return result;
        }
        if (!ParseDoubleStrict(cells[2], average_cost) || average_cost < 0.0)
        {
            result.error = "Average cost must be zero or positive.";
            return result;
        }
        result.row.symbol = symbol.symbol;
        result.row.shares = shares;
        result.row.average_cost = average_cost;
        result.row.note = cells.size() > 3 ? Trim(cells[3]) : "";
        result.ok = true;
        return result;
    }

    AlertImportResult ParseAlertImportRow(const std::vector<std::string>& cells)
    {
        AlertImportResult result;
        if (cells.size() < 3)
        {
            result.error = "Expected symbol, trigger price, and direction.";
            return result;
        }
        const SymbolValidationResult symbol = ValidateTickerSymbol(cells[0]);
        if (!symbol.ok)
        {
            result.error = symbol.reason;
            return result;
        }
        double trigger = 0.0;
        if (!ParseDoubleStrict(cells[1], trigger) || trigger <= 0.0)
        {
            result.error = "Trigger price must be positive.";
            return result;
        }
        result.row.symbol = symbol.symbol;
        result.row.trigger_price = trigger;
        result.row.above = ParseAbove(cells[2]);
        result.row.enabled = cells.size() <= 3 || ParseEnabled(cells[3]);
        result.row.note = cells.size() > 4 ? Trim(cells[4]) : "";
        result.ok = true;
        return result;
    }

    TradePlanImportResult ParseTradePlanImportRow(const std::vector<std::string>& cells)
    {
        TradePlanImportResult result;
        if (cells.size() < 10)
        {
            result.error = "Expected at least 10 trade-plan columns.";
            return result;
        }
        const SymbolValidationResult symbol = ValidateTickerSymbol(cells[1]);
        if (!symbol.ok)
        {
            result.error = symbol.reason;
            return result;
        }
        double entry = 0.0;
        double stop = 0.0;
        double target = 0.0;
        double planned_risk = 0.0;
        double planned_reward = 0.0;
        int shares = 0;
        if (!ParseDoubleStrict(cells[3], entry) || entry <= 0.0)
        {
            result.error = "Entry must be positive.";
            return result;
        }
        if (!ParseDoubleStrict(cells[4], stop) || stop <= 0.0)
        {
            result.error = "Stop must be positive.";
            return result;
        }
        if (!ParseDoubleStrict(cells[5], target) || target <= 0.0)
        {
            result.error = "Target must be positive.";
            return result;
        }
        if (!ParseIntStrict(cells[6], shares) || shares <= 0)
        {
            result.error = "Shares must be a positive integer.";
            return result;
        }
        if (!ParseDoubleStrict(cells[7], planned_risk) || planned_risk < 0.0)
        {
            result.error = "Planned risk must be zero or positive.";
            return result;
        }
        if (!ParseDoubleStrict(cells[8], planned_reward) || planned_reward < 0.0)
        {
            result.error = "Planned reward must be zero or positive.";
            return result;
        }
        result.row.created_at = Trim(cells[0]).empty() ? NowTimeLabel() : Trim(cells[0]);
        result.row.symbol = symbol.symbol;
        result.row.rating = Trim(cells[2]).empty() ? "Watch" : Trim(cells[2]);
        result.row.entry = entry;
        result.row.stop = stop;
        result.row.target = target;
        result.row.shares = shares;
        result.row.planned_risk = planned_risk;
        result.row.planned_reward = planned_reward;
        result.row.status = Trim(cells[9]).empty() ? "Open" : Trim(cells[9]);
        result.row.thesis = cells.size() > 10 ? Trim(cells[10]) : "";
        result.ok = true;
        return result;
    }

    SymbolNoteImportResult ParseSymbolNoteImportRow(const std::vector<std::string>& cells)
    {
        SymbolNoteImportResult result;
        if (cells.size() < 3)
        {
            result.error = "Expected symbol, tags, and note.";
            return result;
        }
        const SymbolValidationResult symbol = ValidateTickerSymbol(cells[0]);
        if (!symbol.ok)
        {
            result.error = symbol.reason;
            return result;
        }
        if (Trim(cells[2]).empty())
        {
            result.error = "Note cannot be empty.";
            return result;
        }
        result.row.symbol = symbol.symbol;
        result.row.tags = Trim(cells[1]);
        result.row.note = Trim(cells[2]);
        result.row.updated_at = cells.size() > 3 && !Trim(cells[3]).empty() ? Trim(cells[3]) : NowTimeLabel();
        result.ok = true;
        return result;
    }

    JournalImportResult ParseJournalImportRow(const std::vector<std::string>& cells)
    {
        JournalImportResult result;
        if (cells.size() < 8)
        {
            result.error = "Expected at least 8 journal columns.";
            return result;
        }
        const SymbolValidationResult symbol = ValidateTickerSymbol(cells[1]);
        if (!symbol.ok)
        {
            result.error = symbol.reason;
            return result;
        }
        if (Trim(cells[2]).empty())
        {
            result.error = "Action cannot be empty.";
            return result;
        }
        double pnl = 0.0;
        if (cells.size() > 8 && !Trim(cells[8]).empty() && !ParseDoubleStrict(cells[8], pnl))
        {
            result.error = "Realized P/L must be numeric.";
            return result;
        }
        result.row.time = Trim(cells[0]).empty() ? NowTimeLabel() : Trim(cells[0]);
        result.row.symbol = symbol.symbol;
        result.row.action = Trim(cells[2]);
        result.row.reason = Trim(cells[3]);
        result.row.exit_reason = Trim(cells[4]);
        result.row.tags = Trim(cells[5]);
        result.row.mistakes = Trim(cells[6]);
        result.row.grade = Trim(cells[7]);
        result.row.realized_pnl = pnl;
        result.ok = true;
        return result;
    }
}
