#include "AlertEngine.h"

#include "Diagnostics.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
        std::vector<AlertEvent> events;
        std::ifstream file(AlertEventsFile());
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
        std::error_code ec;
        std::filesystem::create_directories(AppDataDirectory(), ec);
        std::ofstream file(AlertEventsFile(), std::ios::trunc);
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

    bool IsAlertEventSuppressed(const PriceAlert& alert, const std::vector<AlertEvent>& events)
    {
        return HasOpenEventForKey(AlertKey(alert), events);
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
