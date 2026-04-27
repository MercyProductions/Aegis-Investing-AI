#pragma once

#include "StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct AlertEvent
    {
        std::string id;
        std::string alert_key;
        std::string symbol;
        std::string direction;
        double trigger_price = 0.0;
        double observed_price = 0.0;
        std::string triggered_at;
        std::string source;
        std::string note;
        bool acknowledged = false;
        std::string acknowledged_at;
        long long snoozed_until_epoch = 0;
    };

    std::string AlertKey(const PriceAlert& alert);
    std::vector<AlertEvent> LoadAlertEvents();
    bool SaveAlertEvents(const std::vector<AlertEvent>& events);
    bool IsAlertEventSuppressed(const PriceAlert& alert, const std::vector<AlertEvent>& events);
    AlertEvent MakeAlertEvent(const PriceAlert& alert, const StockQuote& quote);
    void AcknowledgeAlertEvent(AlertEvent& event);
    void SnoozeAlertEvent(AlertEvent& event, int minutes);
    std::string AlertEventState(const AlertEvent& event);
    std::string AlertEventSnoozeLabel(const AlertEvent& event);
}
