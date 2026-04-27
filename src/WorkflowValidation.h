#pragma once

#include "AdvancedAnalytics.h"
#include "StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct HoldingDraftResult
    {
        bool ok = false;
        std::string error;
        PortfolioHolding row;
    };

    struct AlertDraftResult
    {
        bool ok = false;
        std::string error;
        PriceAlert row;
    };

    struct TradePlanDraftResult
    {
        bool ok = false;
        std::string error;
        TradePlan row;
    };

    struct JournalDraftResult
    {
        bool ok = false;
        std::string error;
        JournalEntry row;
    };

    HoldingDraftResult ValidateHoldingDraft(const std::string& symbol, double shares, double average_cost, const std::string& note);
    AlertDraftResult ValidateAlertDraft(const std::string& symbol, double trigger_price, bool above, bool enabled, const std::string& note);
    TradePlanDraftResult ValidateTradePlanDraft(const TradePlan& plan);
    JournalDraftResult ValidateJournalDraft(const std::string& time,
        const std::string& symbol,
        const std::string& action,
        const std::string& reason,
        const std::string& exit_reason,
        const std::string& tags,
        const std::string& mistakes,
        const std::string& grade,
        double realized_pnl);

    int FindDuplicateHoldingIndex(const std::vector<PortfolioHolding>& holdings, const std::string& symbol, int ignore_index);
    int FindDuplicateAlertIndex(const std::vector<PriceAlert>& alerts, const PriceAlert& alert, int ignore_index);
}
