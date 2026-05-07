#pragma once

#include "../Platform.h"
#include "../StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    enum class ExecutionIntent
    {
        ResearchSimulation,
        PaperExecution,
        LiveExecution
    };

    struct SafetyGateInput
    {
        ExecutionIntent intent = ExecutionIntent::ResearchSimulation;
        bool manual_confirmed = false;
        std::string symbol;
    };

    struct SafetyGateResult
    {
        bool allowed = false;
        std::string state;
        std::string reason;
        std::string audit_action;
    };

    SafetyGateResult EvaluateSafetyGate(const Config& config, const SafetyGateInput& input);
    std::string CurrentRiskStateLabel(const Config& config);
    std::vector<InfoItem> BuildSafetyGateRows(const Config& config);
}
