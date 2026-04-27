#include "AppStorage.h"

#include "Diagnostics.h"
#include "Platform.h"

#include <string>
#include <vector>

namespace aegis
{
    PersistentAppData LoadPersistentAppData()
    {
        PersistentAppData data;
        data.holdings = LoadPortfolioHoldings();
        data.price_alerts = LoadPriceAlerts();
        data.alert_events = LoadAlertEvents();
        data.trade_plans = LoadTradePlans();
        data.symbol_notes = LoadSymbolNotes();
        data.journal_entries = LoadJournalEntries();
        return data;
    }

    bool SavePersistentAppData(const PersistentAppData& data, std::string* status)
    {
        bool ok = true;
        std::vector<std::string> failures;

        const auto save = [&](bool saved, const char* name) {
            if (!saved)
            {
                ok = false;
                failures.push_back(name);
            }
        };

        save(SavePortfolioHoldings(data.holdings), "holdings");
        save(SavePriceAlerts(data.price_alerts), "price alerts");
        save(SaveAlertEvents(data.alert_events), "alert events");
        save(SaveTradePlans(data.trade_plans), "trade plans");
        save(SaveSymbolNotes(data.symbol_notes), "symbol notes");
        save(SaveJournalEntries(data.journal_entries), "journal entries");

        if (status != nullptr)
        {
            if (ok)
                *status = "Saved persistent app data.";
            else
            {
                *status = "Could not save: ";
                for (size_t i = 0; i < failures.size(); ++i)
                {
                    if (i > 0)
                        *status += ", ";
                    *status += failures[i];
                }
            }
        }

        if (!ok)
        {
            std::string detail;
            for (size_t i = 0; i < failures.size(); ++i)
            {
                if (i > 0)
                    detail += ",";
                detail += failures[i];
            }
            AppendDiagnosticEvent({ "error", "storage", "SavePersistentAppData", "", "failed", detail, AppDataDirectory().string(), 0, 0, false });
        }

        return ok;
    }

    std::vector<InfoItem> BuildPersistentDataRows(const PersistentAppData& data)
    {
        return {
            { "Holdings", "Legacy file", std::to_string(data.holdings.size()), "", "Saved portfolio positions ready for SQLite migration.", "storage" },
            { "Price alerts", "Legacy file", std::to_string(data.price_alerts.size()), "", "Configured alert rules.", "storage" },
            { "Alert events", "Legacy file", std::to_string(data.alert_events.size()), "", "Trigger history, acknowledgement, and snooze state.", "storage" },
            { "Trade plans", "Legacy file", std::to_string(data.trade_plans.size()), "", "Paper trade plans and status tracking.", "storage" },
            { "Symbol notes", "Legacy file", std::to_string(data.symbol_notes.size()), "", "Research notes and tags by ticker.", "storage" },
            { "Journal entries", "Legacy file", std::to_string(data.journal_entries.size()), "", "Trade journal records and mistake tags.", "storage" }
        };
    }
}
