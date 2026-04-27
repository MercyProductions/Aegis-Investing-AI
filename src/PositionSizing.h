#pragma once

#include <string>

namespace aegis
{
    enum class PositionSizingMode
    {
        FixedDollar = 0,
        FixedRisk = 1,
        VolatilityAdjusted = 2,
        AtrStop = 3,
        KellyLite = 4
    };

    struct PositionSizingInput
    {
        PositionSizingMode mode = PositionSizingMode::FixedRisk;
        double portfolio_cash = 0.0;
        double max_position_amount = 0.0;
        double max_portfolio_risk_percent = 0.0;
        double entry = 0.0;
        double stop = 0.0;
        double target = 0.0;
        double beta = 1.0;
        double change_percent = 0.0;
        int score = 50;
        double atr14 = 0.0;
    };

    struct PositionSizingResult
    {
        bool ok = false;
        int shares = 0;
        double notional = 0.0;
        double planned_risk = 0.0;
        double planned_reward = 0.0;
        double effective_stop = 0.0;
        double risk_per_share = 0.0;
        std::string reason;
    };

    PositionSizingMode PositionSizingModeFromIndex(int mode);
    PositionSizingResult CalculatePositionSize(const PositionSizingInput& input);
}
