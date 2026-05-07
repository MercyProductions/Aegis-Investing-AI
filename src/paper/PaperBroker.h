#pragma once

#include "../StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    enum class PaperOrderSide
    {
        Buy,
        Sell
    };

    enum class PaperOrderState
    {
        Pending,
        Queued,
        PartiallyFilled,
        Filled,
        Cancelled,
        Rejected,
        Expired
    };

    struct PaperOrderRequest
    {
        std::string symbol;
        PaperOrderSide side = PaperOrderSide::Buy;
        double shares = 0.0;
        double limit_price = 0.0;
        double max_slippage_bps = 25.0;
        bool manual_confirmed = false;
        bool force_live = false;
    };

    struct PaperFill
    {
        double shares = 0.0;
        double price = 0.0;
        double slippage_bps = 0.0;
        double commission = 0.0;
        int latency_ms = 0;
        std::string detail;
    };

    struct PaperOrderResult
    {
        bool accepted = false;
        bool blocked = false;
        PaperOrderState state = PaperOrderState::Pending;
        double requested_shares = 0.0;
        double filled_shares = 0.0;
        double average_fill_price = 0.0;
        double notional = 0.0;
        double commission = 0.0;
        int latency_ms = 0;
        std::string audit_action;
        std::string detail;
        std::vector<InfoItem> rows;
    };

    const char* PaperOrderStateName(PaperOrderState state);
    double EstimateSlippageBps(const StockQuote& quote, const PaperOrderRequest& request);
    double EstimatePaperCommission(const PaperOrderRequest& request);
    PaperFill SimulatePaperFill(const StockQuote& quote, const PaperOrderRequest& request);
    PaperOrderResult SubmitPaperOrder(const Config& config, const StockQuote& quote, const PaperOrderRequest& request);
    std::vector<InfoItem> BuildExecutionAuditRows(const PaperOrderResult& result);
}
