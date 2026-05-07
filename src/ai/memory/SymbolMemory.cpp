#include "IntelligenceMemory.h"

#include "../../Platform.h"

#include <algorithm>

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
            item.tag = "ai-memory";
            return item;
        }
    }

    std::vector<InfoItem> BuildSymbolMemoryRows(const StockState& state, const std::vector<SymbolNote>& notes, const std::vector<JournalEntry>& journal, const std::string& symbol)
    {
        std::vector<InfoItem> rows;
        const StockQuote* quote = FindQuote(state, symbol);
        const StockSignal* signal = FindSignal(state, symbol);
        int note_count = 0;
        int journal_count = 0;
        int mistake_count = 0;
        for (const SymbolNote& note : notes)
        {
            if (Upper(note.symbol) == Upper(symbol))
                ++note_count;
        }
        for (const JournalEntry& entry : journal)
        {
            if (Upper(entry.symbol) == Upper(symbol))
            {
                ++journal_count;
                if (!Trim(entry.mistakes).empty())
                    ++mistake_count;
            }
        }
        rows.push_back(Row("Symbol memory", symbol.empty() ? "No symbol" : symbol, quote != nullptr ? FormatCurrency(quote->price) : "--", quote != nullptr ? "Current quote state anchors this symbol's memory." : "No current quote found.", quote != nullptr ? "Ready" : "Review"));
        rows.push_back(Row("Prior notes", note_count > 0 ? "Remembered" : "Missing", std::to_string(note_count), "Symbol notes become durable research memory."));
        rows.push_back(Row("Journal history", journal_count > 0 ? "Remembered" : "No entries", std::to_string(journal_count), "Journal entries help identify repeated setup behavior and review patterns."));
        rows.push_back(Row("Repeated mistakes", mistake_count > 0 ? "Review" : "Clear", std::to_string(mistake_count), "Mistake notes should inform future invalidation and sizing warnings.", mistake_count > 0 ? "Review" : "Ready"));
        if (signal != nullptr)
            rows.push_back(Row("Current setup memory", signal->rating, std::to_string(signal->score), "This setup should be compared to prior scanner outcomes once scanner_history is populated."));
        return rows;
    }
}
