#include "ResearchTimeline.h"

#include "../Platform.h"

#include <algorithm>

namespace aegis
{
    namespace
    {
        InfoItem Row(const ResearchTimelineEntry& entry)
        {
            InfoItem item;
            item.name = entry.type;
            item.label = entry.symbol;
            item.value = entry.time;
            item.detail = entry.title + ". " + entry.detail;
            item.source = entry.source;
            item.time = entry.time;
            item.state = "Ready";
            item.tag = "timeline";
            return item;
        }
    }

    std::vector<ResearchTimelineEntry> BuildResearchTimeline(const StockState& state, const std::string& symbol, const std::vector<AlertEvent>& alerts, const std::vector<TradePlan>& plans, const std::vector<JournalEntry>& journal, const std::vector<SymbolNote>& notes)
    {
        std::vector<ResearchTimelineEntry> entries;
        const std::string target = Upper(symbol);
        if (const StockQuote* quote = FindQuote(state, target))
        {
            entries.push_back({ quote->timestamp.empty() ? state.last_refresh_label : quote->timestamp, quote->symbol, "Quote", "Quote observed", FormatCurrency(quote->price) + " from " + (quote->source.empty() ? state.source_badge : quote->source), quote->source });
        }
        if (const StockSignal* signal = FindSignal(state, target))
        {
            entries.push_back({ state.last_refresh_label, signal->symbol, "Scanner Signal", signal->rating + " / " + std::to_string(signal->score), signal->thesis, signal->source });
        }
        for (const AlertEvent& event : alerts)
        {
            if (Upper(event.symbol) == target)
                entries.push_back({ event.triggered_at, event.symbol, "Alert", event.direction + " " + FormatCurrency(event.trigger_price), event.note.empty() ? "Alert event was triggered." : event.note, event.source });
        }
        for (const TradePlan& plan : plans)
        {
            if (Upper(plan.symbol) == target)
                entries.push_back({ plan.created_at, plan.symbol, "Paper Plan", plan.rating + " / " + plan.status, "Entry " + FormatCurrency(plan.entry) + ", stop " + FormatCurrency(plan.stop) + ", target " + FormatCurrency(plan.target) + ". " + plan.thesis, "paper-plan" });
        }
        for (const JournalEntry& entry : journal)
        {
            if (Upper(entry.symbol) == target)
                entries.push_back({ entry.time, entry.symbol, "Journal", entry.action + " / " + entry.grade, entry.reason + (entry.mistakes.empty() ? "" : " Mistake: " + entry.mistakes), "journal" });
        }
        for (const SymbolNote& note : notes)
        {
            if (Upper(note.symbol) == target)
                entries.push_back({ note.updated_at, note.symbol, "Research Note", note.tags, note.note, "symbol-note" });
        }
        std::sort(entries.begin(), entries.end(), [](const ResearchTimelineEntry& left, const ResearchTimelineEntry& right) {
            return left.time > right.time;
        });
        return entries;
    }

    std::vector<InfoItem> BuildResearchTimelineRows(const StockState& state, const std::string& symbol, const std::vector<AlertEvent>& alerts, const std::vector<TradePlan>& plans, const std::vector<JournalEntry>& journal, const std::vector<SymbolNote>& notes)
    {
        std::vector<InfoItem> rows;
        for (const ResearchTimelineEntry& entry : BuildResearchTimeline(state, symbol, alerts, plans, journal, notes))
            rows.push_back(Row(entry));
        if (rows.empty())
        {
            InfoItem item;
            item.name = "Research Timeline";
            item.label = symbol.empty() ? "No symbol" : symbol;
            item.value = "--";
            item.detail = "No timeline entries found yet. Alerts, filings, scanner signals, journal notes, reports, paper trades, and strategy interactions will build continuity over time.";
            item.state = "Review";
            item.tag = "timeline";
            rows.push_back(item);
        }
        return rows;
    }
}
