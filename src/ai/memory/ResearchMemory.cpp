#include "IntelligenceMemory.h"

#include "../../Platform.h"

namespace aegis
{
    namespace
    {
        InfoItem Row(const std::string& name, const std::string& label, const std::string& value, const std::string& detail, const std::string& state = "Ready")
        {
            InfoItem item;
            item.name = name;
            item.label = label;
            item.value = value;
            item.detail = detail;
            item.state = state;
            item.tag = "research-memory";
            return item;
        }
    }

    std::vector<InfoItem> BuildResearchMemoryRows(const StockState& state, const std::vector<TradePlan>& plans, const std::vector<JournalEntry>& journal)
    {
        int open_plans = 0;
        int stopped_or_closed = 0;
        for (const TradePlan& plan : plans)
        {
            const std::string status = Lower(plan.status);
            if (status.find("open") != std::string::npos)
                ++open_plans;
            if (status.find("closed") != std::string::npos || status.find("stop") != std::string::npos)
                ++stopped_or_closed;
        }
        return {
            Row("Scanner memory", "Planned", std::to_string(static_cast<int>(state.signals.size())), "scanner_history will preserve prior scores and outcomes so new setups can be compared to old ones."),
            Row("Paper plan memory", open_plans > 0 ? "Active" : "Quiet", std::to_string(open_plans), "Open paper plans become research continuity anchors."),
            Row("Failed setup memory", stopped_or_closed > 0 ? "Review" : "Learning", std::to_string(stopped_or_closed), "Closed/stopped plans should teach future setup comparisons.", stopped_or_closed > 0 ? "Review" : "Ready"),
            Row("Journal memory", journal.empty() ? "No entries" : "Available", std::to_string(static_cast<int>(journal.size())), "Journal text powers cautious assistant-style review prompts.")
        };
    }
}
