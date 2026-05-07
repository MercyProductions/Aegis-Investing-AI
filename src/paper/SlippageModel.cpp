#include "PaperBroker.h"

#include <algorithm>
#include <cmath>

namespace aegis
{
    double EstimateSlippageBps(const StockQuote& quote, const PaperOrderRequest& request)
    {
        const double range_percent = quote.price > 0.0
            ? std::fabs(quote.high - quote.low) / quote.price * 100.0
            : 0.0;
        const double size_pressure = quote.volume > 0
            ? std::min(35.0, (request.shares / std::max(1.0, static_cast<double>(quote.volume))) * 100000.0)
            : 18.0;
        const double liquidity_floor = quote.volume > 0 && quote.volume < 500000 ? 14.0 : 4.0;
        return std::clamp(liquidity_floor + range_percent * 2.0 + size_pressure, 1.0, request.max_slippage_bps);
    }
}
