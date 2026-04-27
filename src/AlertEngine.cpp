#include "AlertEngine.h"

#include "Diagnostics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace aegis
{
    namespace
    {
        std::filesystem::path AlertEventsFile()
        {
            return AppDataDirectory() / "alert-events.tsv";
        }

        long long NowEpoch()
        {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        std::string NowStamp()
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm local{};
            localtime_s(&local, &tt);
            std::ostringstream stream;
            stream << std::put_time(&local, "%Y-%m-%d %H:%M");
            return stream.str();
        }

        std::string TsvField(std::string value)
        {
            std::replace(value.begin(), value.end(), '\t', ' ');
            std::replace(value.begin(), value.end(), '\r', ' ');
            std::replace(value.begin(), value.end(), '\n', ' ');
            return value;
        }

        std::vector<std::string> SplitTabs(const std::string& line)
        {
            std::vector<std::string> parts;
            std::stringstream stream(line);
            std::string part;
            while (std::getline(stream, part, '\t'))
                parts.push_back(part);
            return parts;
        }

        double ParseDouble(const std::string& value)
        {
            try
            {
                return std::stod(value);
            }
            catch (...)
            {
                return 0.0;
            }
        }

        long long ParseLongLong(const std::string& value)
        {
            try
            {
                return std::stoll(value);
            }
            catch (...)
            {
                return 0;
            }
        }

        bool ParseBool(const std::string& value)
        {
            const std::string lowered = Lower(Trim(value));
            return lowered == "true" || lowered == "1" || lowered == "yes";
        }

        bool HasOpenEventForKey(const std::string& key, const std::vector<AlertEvent>& events)
        {
            const long long now = NowEpoch();
            for (const AlertEvent& event : events)
            {
                if (event.alert_key != key)
                    continue;
                if (!event.acknowledged)
                    return true;
                if (event.snoozed_until_epoch > now)
                    return true;
            }
            return false;
        }
    }

    std::string AlertKey(const PriceAlert& alert)
    {
        std::ostringstream stream;
        stream << Upper(Trim(alert.symbol)) << "|" << (alert.above ? "above" : "below") << "|";
        stream << std::fixed << std::setprecision(2) << alert.trigger_price;
        return stream.str();
    }

    std::vector<AlertEvent> LoadAlertEvents()
    {
        return LoadAlertEvents(AlertEventsFile());
    }

    std::vector<AlertEvent> LoadAlertEvents(const std::filesystem::path& path)
    {
        std::vector<AlertEvent> events;
        std::ifstream file(path);
        if (!file)
            return events;
        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const std::vector<std::string> parts = SplitTabs(line);
            if (parts.size() < 12 || Lower(parts[0]) == "id")
                continue;
            AlertEvent event;
            event.id = parts[0];
            event.alert_key = parts[1];
            event.symbol = Upper(Trim(parts[2]));
            event.direction = Trim(parts[3]);
            event.trigger_price = ParseDouble(parts[4]);
            event.observed_price = ParseDouble(parts[5]);
            event.triggered_at = parts[6];
            event.source = parts[7];
            event.note = parts[8];
            event.acknowledged = ParseBool(parts[9]);
            event.acknowledged_at = parts[10];
            event.snoozed_until_epoch = ParseLongLong(parts[11]);
            if (!event.symbol.empty())
                events.push_back(event);
        }
        return events;
    }

    bool SaveAlertEvents(const std::vector<AlertEvent>& events)
    {
        return SaveAlertEvents(events, AlertEventsFile());
    }

    bool SaveAlertEvents(const std::vector<AlertEvent>& events, const std::filesystem::path& path)
    {
        std::error_code ec;
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::trunc);
        if (!file)
            return false;
        file << "id\talert_key\tsymbol\tdirection\ttrigger_price\tobserved_price\ttriggered_at\tsource\tnote\tacknowledged\tacknowledged_at\tsnoozed_until_epoch\n";
        for (const AlertEvent& event : events)
        {
            file << TsvField(event.id) << '\t'
                 << TsvField(event.alert_key) << '\t'
                 << TsvField(event.symbol) << '\t'
                 << TsvField(event.direction) << '\t'
                 << event.trigger_price << '\t'
                 << event.observed_price << '\t'
                 << TsvField(event.triggered_at) << '\t'
                 << TsvField(event.source) << '\t'
                 << TsvField(event.note) << '\t'
                 << (event.acknowledged ? "true" : "false") << '\t'
                 << TsvField(event.acknowledged_at) << '\t'
                 << event.snoozed_until_epoch << '\n';
        }
        return true;
    }

    bool IsPriceAlertTriggered(const PriceAlert& alert, const StockQuote& quote)
    {
        if (!alert.enabled || quote.price <= 0.0)
            return false;
        return alert.above ? quote.price >= alert.trigger_price : quote.price <= alert.trigger_price;
    }

    bool IsAlertEventSuppressed(const PriceAlert& alert, const std::vector<AlertEvent>& events)
    {
        return HasOpenEventForKey(AlertKey(alert), events);
    }

    AlertEvaluationResult EvaluatePriceAlerts(const std::vector<PriceAlert>& alerts, const std::vector<StockQuote>& quotes, const std::vector<AlertEvent>& events, const std::string& source_label)
    {
        AlertEvaluationResult result;
        result.source = source_label;
        std::map<std::string, const StockQuote*> quote_by_symbol;
        for (const StockQuote& quote : quotes)
        {
            if (!quote.symbol.empty())
                quote_by_symbol[Upper(Trim(quote.symbol))] = &quote;
        }

        for (const PriceAlert& alert : alerts)
        {
            if (!alert.enabled)
                continue;
            ++result.checked;
            const std::string symbol = Upper(Trim(alert.symbol));
            const auto quote_it = quote_by_symbol.find(symbol);
            if (quote_it == quote_by_symbol.end() || quote_it->second == nullptr || quote_it->second->price <= 0.0)
            {
                ++result.missing_quotes;
                continue;
            }

            const StockQuote& quote = *quote_it->second;
            if (!IsPriceAlertTriggered(alert, quote))
                continue;

            ++result.triggered;
            const bool suppressed = IsAlertEventSuppressed(alert, events);
            if (suppressed)
                ++result.suppressed;

            const std::string direction = alert.above ? "above" : "below";
            InfoItem row;
            row.name = symbol;
            row.label = suppressed ? "Custom price alert active" : "Custom price alert triggered";
            row.value = FormatCurrency(quote.price);
            row.detail = symbol + " is " + direction + " " + FormatCurrency(alert.trigger_price) + ". " + (suppressed ? "Existing event is open/snoozed. " : "New alert event recorded. ") + alert.note;
            row.tag = "Custom";
            row.state = quote.source.empty() ? source_label : quote.source;
            row.source = row.state;
            row.time = NowStamp();
            result.rows.push_back(std::move(row));

            if (!suppressed)
                result.new_events.push_back(MakeAlertEvent(alert, quote));
        }

        std::ostringstream summary;
        summary << "Alert monitor checked " << result.checked << " enabled alert(s), "
                << result.triggered << " triggered, "
                << result.new_events.size() << " new, "
                << result.suppressed << " already open/snoozed";
        if (result.missing_quotes > 0)
            summary << ", " << result.missing_quotes << " missing quotes";
        if (!result.source.empty())
            summary << " using " << result.source;
        summary << ".";
        result.summary = summary.str();
        return result;
    }

    AlertEvent MakeAlertEvent(const PriceAlert& alert, const StockQuote& quote)
    {
        AlertEvent event;
        event.id = std::to_string(NowEpoch()) + "-" + Upper(alert.symbol);
        event.alert_key = AlertKey(alert);
        event.symbol = Upper(alert.symbol);
        event.direction = alert.above ? "above" : "below";
        event.trigger_price = alert.trigger_price;
        event.observed_price = quote.price;
        event.triggered_at = NowStamp();
        event.source = quote.source;
        event.note = alert.note;
        return event;
    }

    void AcknowledgeAlertEvent(AlertEvent& event)
    {
        event.acknowledged = true;
        event.acknowledged_at = NowStamp();
        event.snoozed_until_epoch = 0;
        AppendAuditEvent({ "local", "alert_acknowledged", event.symbol, event.alert_key });
    }

    void SnoozeAlertEvent(AlertEvent& event, int minutes)
    {
        event.acknowledged = false;
        event.snoozed_until_epoch = NowEpoch() + static_cast<long long>(std::max(1, minutes)) * 60;
        AppendAuditEvent({ "local", "alert_snoozed", event.symbol, std::to_string(std::max(1, minutes)) + " minutes" });
    }

    int CountUnreadAlertEvents(const std::vector<AlertEvent>& events)
    {
        int count = 0;
        for (const AlertEvent& event : events)
        {
            if (!event.acknowledged)
                ++count;
        }
        return count;
    }

    AlertOutcome AlertOutcomeForEvent(const AlertEvent& event, const std::vector<StockQuote>& quotes)
    {
        AlertOutcome outcome;
        const std::string symbol = Upper(Trim(event.symbol));
        for (const StockQuote& quote : quotes)
        {
            if (Upper(Trim(quote.symbol)) != symbol || quote.price <= 0.0)
                continue;

            outcome.has_quote = true;
            outcome.current_price = quote.price;
            outcome.change = quote.price - event.observed_price;
            outcome.change_percent = event.observed_price > 0.0 ? (outcome.change / event.observed_price) * 100.0 : 0.0;
            const double abs_change = std::fabs(outcome.change_percent);
            if (abs_change < 0.10)
                outcome.label = "Flat";
            else if (event.direction == "below")
                outcome.label = outcome.change <= 0.0 ? "Extended lower" : "Rebounded";
            else
                outcome.label = outcome.change >= 0.0 ? "Extended" : "Pulled back";
            outcome.detail = "Triggered at " + FormatCurrency(event.observed_price) + ", current " + FormatCurrency(outcome.current_price) + ", change " + FormatPercent(outcome.change_percent) + ".";
            return outcome;
        }

        outcome.label = "No quote";
        outcome.detail = "Current quote unavailable for outcome tracking.";
        return outcome;
    }

    std::vector<InfoItem> BuildAlertNotificationRows(const std::vector<AlertEvent>& events, const std::vector<StockQuote>& quotes, size_t max_rows)
    {
        std::vector<InfoItem> rows;
        for (auto it = events.rbegin(); it != events.rend(); ++it)
        {
            const AlertEvent& event = *it;
            if (event.acknowledged)
                continue;
            const AlertOutcome outcome = AlertOutcomeForEvent(event, quotes);
            InfoItem row;
            row.name = event.symbol;
            row.state = AlertEventState(event);
            row.value = outcome.has_quote ? outcome.label + " " + FormatPercent(outcome.change_percent) : outcome.label;
            row.detail = event.triggered_at + " | " + event.direction + " " + FormatCurrency(event.trigger_price) + " | " + outcome.detail;
            row.source = event.source;
            row.time = event.triggered_at;
            rows.push_back(std::move(row));
            if (rows.size() >= std::max<size_t>(1, max_rows))
                break;
        }
        if (rows.empty())
        {
            InfoItem row;
            row.name = "Notifications";
            row.state = "Clear";
            row.value = "0 unread";
            row.detail = "No open alert notifications.";
            rows.push_back(std::move(row));
        }
        return rows;
    }

    std::string AlertEventState(const AlertEvent& event)
    {
        if (event.acknowledged)
            return "Acknowledged";
        if (event.snoozed_until_epoch > NowEpoch())
            return "Snoozed";
        return "Open";
    }

    std::string AlertEventSnoozeLabel(const AlertEvent& event)
    {
        const long long now = NowEpoch();
        if (event.snoozed_until_epoch <= now)
            return "";
        const long long minutes = std::max(1LL, (event.snoozed_until_epoch - now + 59) / 60);
        return std::to_string(minutes) + " min left";
    }
}
