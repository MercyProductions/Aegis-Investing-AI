#include "PaperBroker.h"

#include <algorithm>

namespace aegis
{
    PaperFill SimulatePaperFill(const StockQuote& quote, const PaperOrderRequest& request)
    {
        PaperFill fill;
        fill.slippage_bps = EstimateSlippageBps(quote, request);
        const double side_adjustment = request.side == PaperOrderSide::Buy ? 1.0 : -1.0;
        const double reference = request.limit_price > 0.0 ? request.limit_price : quote.price;
        fill.price = reference * (1.0 + side_adjustment * fill.slippage_bps / 10000.0);
        const double max_fillable = quote.volume > 0
            ? std::max(1.0, static_cast<double>(quote.volume) * 0.002)
            : std::max(1.0, request.shares * 0.55);
        fill.shares = std::min(request.shares, max_fillable);
        fill.commission = EstimatePaperCommission(request);
        fill.latency_ms = quote.live ? 250 : 900;
        fill.detail = fill.shares < request.shares
            ? "Liquidity simulation produced a partial fill."
            : "Paper fill simulated with slippage, commission, and latency.";
        return fill;
    }
}
