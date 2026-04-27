#include "PositionSizing.h"

#include <algorithm>
#include <cmath>

namespace aegis
{
    PositionSizingMode PositionSizingModeFromIndex(int mode)
    {
        switch (mode)
        {
        case 0:
            return PositionSizingMode::FixedDollar;
        case 2:
            return PositionSizingMode::VolatilityAdjusted;
        case 3:
            return PositionSizingMode::AtrStop;
        case 4:
            return PositionSizingMode::KellyLite;
        case 1:
        default:
            return PositionSizingMode::FixedRisk;
        }
    }

    PositionSizingResult CalculatePositionSize(const PositionSizingInput& input)
    {
        PositionSizingResult result;
        if (input.entry <= 0.0 || !std::isfinite(input.entry))
        {
            result.reason = "Entry must be positive.";
            return result;
        }
        if (input.portfolio_cash <= 0.0 || input.max_position_amount <= 0.0)
        {
            result.reason = "Portfolio cash and max position must be positive.";
            return result;
        }

        const double price = std::max(0.01, input.entry);
        result.effective_stop = input.stop;
        if (input.mode == PositionSizingMode::AtrStop)
            result.effective_stop = std::max(0.01, price - std::max(0.0, input.atr14) * 1.5);

        const double risk_dollars = input.portfolio_cash * (std::max(0.0, input.max_portfolio_risk_percent) / 100.0);
        result.risk_per_share = std::max(0.01, price - result.effective_stop);
        const int shares_by_risk = std::max(0, static_cast<int>(std::floor(risk_dollars / result.risk_per_share)));
        const int shares_by_cap = std::max(0, static_cast<int>(std::floor(input.max_position_amount / price)));

        if (input.mode == PositionSizingMode::FixedDollar)
        {
            result.shares = shares_by_cap;
        }
        else if (input.mode == PositionSizingMode::VolatilityAdjusted)
        {
            const double volatility_divisor = std::clamp(input.beta + std::fabs(input.change_percent) / 6.0, 0.65, 2.50);
            result.shares = std::min(shares_by_risk, std::max(0, static_cast<int>(std::floor((input.max_position_amount / volatility_divisor) / price))));
        }
        else if (input.mode == PositionSizingMode::KellyLite)
        {
            const double reward_per_share = std::max(0.01, input.target - price);
            const double rr = reward_per_share / result.risk_per_share;
            const double probability = std::clamp(0.38 + static_cast<double>(input.score) / 300.0, 0.40, 0.68);
            const double kelly = std::clamp((probability * rr - (1.0 - probability)) / std::max(0.01, rr), 0.0, 0.06);
            result.shares = std::min(shares_by_risk, std::max(0, static_cast<int>(std::floor((input.portfolio_cash * kelly) / price))));
        }
        else
        {
            result.shares = std::min(shares_by_risk, shares_by_cap);
        }

        result.notional = static_cast<double>(result.shares) * price;
        result.planned_risk = static_cast<double>(result.shares) * result.risk_per_share;
        result.planned_reward = static_cast<double>(result.shares) * std::max(0.0, input.target - price);
        result.ok = result.shares > 0;
        if (!result.ok)
            result.reason = "Sizing produced zero shares.";
        return result;
    }
}
