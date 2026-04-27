#pragma once

#include "StockData.h"

#include <filesystem>
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

    struct AlertEvaluationResult
    {
        int checked = 0;
        int triggered = 0;
        int suppressed = 0;
        int missing_quotes = 0;
        std::string source;
        std::string summary;
        std::vector<AlertEvent> new_events;
        std::vector<InfoItem> rows;
    };

    struct AlertOutcome
    {
        bool has_quote = false;
        double current_price = 0.0;
        double change = 0.0;
        double change_percent = 0.0;
        std::string label;
        std::string detail;
    };

    std::string AlertKey(const PriceAlert& alert);
    std::vector<AlertEvent> LoadAlertEvents();
    std::vector<AlertEvent> LoadAlertEvents(const std::filesystem::path& path);
    bool SaveAlertEvents(const std::vector<AlertEvent>& events);
    bool SaveAlertEvents(const std::vector<AlertEvent>& events, const std::filesystem::path& path);
    bool IsPriceAlertTriggered(const PriceAlert& alert, const StockQuote& quote);
    bool IsAlertEventSuppressed(const PriceAlert& alert, const std::vector<AlertEvent>& events);
    AlertEvaluationResult EvaluatePriceAlerts(const std::vector<PriceAlert>& alerts, const std::vector<StockQuote>& quotes, const std::vector<AlertEvent>& events, const std::string& source_label);
    AlertEvent MakeAlertEvent(const PriceAlert& alert, const StockQuote& quote);
    void AcknowledgeAlertEvent(AlertEvent& event);
    void SnoozeAlertEvent(AlertEvent& event, int minutes);
    int CountUnreadAlertEvents(const std::vector<AlertEvent>& events);
    AlertOutcome AlertOutcomeForEvent(const AlertEvent& event, const std::vector<StockQuote>& quotes);
    std::vector<InfoItem> BuildAlertNotificationRows(const std::vector<AlertEvent>& events, const std::vector<StockQuote>& quotes, size_t max_rows);
    std::string AlertEventState(const AlertEvent& event);
    std::string AlertEventSnoozeLabel(const AlertEvent& event);
}
