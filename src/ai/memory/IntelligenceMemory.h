#pragma once

#include "../../AdvancedAnalytics.h"

#include <string>
#include <vector>

namespace aegis
{
    struct MemoryObservation
    {
        std::string scope;
        std::string symbol;
        std::string type;
        std::string summary;
        std::string evidence;
        double confidence = 0.5;
    };

    std::vector<InfoItem> BuildSymbolMemoryRows(const StockState& state, const std::vector<SymbolNote>& notes, const std::vector<JournalEntry>& journal, const std::string& symbol);
    std::vector<InfoItem> BuildPortfolioMemoryRows(const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<JournalEntry>& journal);
    std::vector<InfoItem> BuildResearchMemoryRows(const StockState& state, const std::vector<TradePlan>& plans, const std::vector<JournalEntry>& journal);
    std::vector<InfoItem> BuildSessionMemoryRows(const Config& config, const StockState& state);
}
