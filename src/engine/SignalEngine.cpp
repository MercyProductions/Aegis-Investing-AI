#include "SignalEngine.h"

#include "../Platform.h"

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
            item.tag = "signal-engine";
            return item;
        }
    }

    SignalEngineSnapshot BuildSignalEngineSnapshot(const StockState& state)
    {
        SignalEngineSnapshot snapshot;
        snapshot.signal_count = static_cast<int>(state.signals.size());
        int score_total = 0;
        for (const StockSignal& signal : state.signals)
        {
            score_total += signal.score;
            if (signal.score >= 76)
                ++snapshot.high_conviction;
            if (Lower(signal.risk).find("risk") != std::string::npos || signal.score < 45)
                ++snapshot.caution_count;
        }
        snapshot.average_score = snapshot.signal_count > 0 ? static_cast<double>(score_total) / snapshot.signal_count : 0.0;

        std::vector<StockSignal> ranked = state.signals;
        std::sort(ranked.begin(), ranked.end(), [](const StockSignal& a, const StockSignal& b) {
            return a.score > b.score;
        });
        const std::string top = ranked.empty() ? "--" : ranked.front().symbol + " " + std::to_string(ranked.front().score);

        snapshot.rows = {
            Row("Signal universe", "Ranked", std::to_string(snapshot.signal_count), "Scanner scoring, conviction, trend, and risk labels are exposed through SignalEngine."),
            Row("Average conviction", snapshot.average_score >= 62.0 ? "Constructive" : "Neutral", FormatPercent(snapshot.average_score), "Average score across the current research board."),
            Row("High-conviction set", snapshot.high_conviction > 0 ? "Active" : "Quiet", std::to_string(snapshot.high_conviction), "Signals over the high-conviction threshold still require manual research review."),
            Row("Caution set", snapshot.caution_count == 0 ? "Clear" : "Review", std::to_string(snapshot.caution_count), "Weak, noisy, or explicitly risky rows are separated from opportunity triage.", snapshot.caution_count == 0 ? "Ready" : "Review"),
            Row("Top ranked", ranked.empty() ? "Empty" : ranked.front().rating, top, ranked.empty() ? "No ranked signal available." : ranked.front().thesis, ranked.empty() ? "Review" : "Ready")
        };
        return snapshot;
    }

    std::vector<InfoItem> BuildSignalEngineRows(const StockState& state)
    {
        return BuildSignalEngineSnapshot(state).rows;
    }
}
