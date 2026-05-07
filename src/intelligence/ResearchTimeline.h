#pragma once

#include "../AdvancedAnalytics.h"
#include "../AlertEngine.h"

#include <string>
#include <vector>

namespace aegis
{
    struct ResearchTimelineEntry
    {
        std::string time;
        std::string symbol;
        std::string type;
        std::string title;
        std::string detail;
        std::string source;
    };

    std::vector<ResearchTimelineEntry> BuildResearchTimeline(const StockState& state, const std::string& symbol, const std::vector<AlertEvent>& alerts, const std::vector<TradePlan>& plans, const std::vector<JournalEntry>& journal, const std::vector<SymbolNote>& notes);
    std::vector<InfoItem> BuildResearchTimelineRows(const StockState& state, const std::string& symbol, const std::vector<AlertEvent>& alerts, const std::vector<TradePlan>& plans, const std::vector<JournalEntry>& journal, const std::vector<SymbolNote>& notes);
}
