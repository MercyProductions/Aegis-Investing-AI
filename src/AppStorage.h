#pragma once

#include "AdvancedAnalytics.h"
#include "AlertEngine.h"
#include "StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct PersistentAppData
    {
        std::vector<PortfolioHolding> holdings;
        std::vector<PriceAlert> price_alerts;
        std::vector<AlertEvent> alert_events;
        std::vector<TradePlan> trade_plans;
        std::vector<SymbolNote> symbol_notes;
        std::vector<JournalEntry> journal_entries;
    };

    PersistentAppData LoadPersistentAppData();
    bool SavePersistentAppData(const PersistentAppData& data, std::string* status = nullptr);
    std::vector<InfoItem> BuildPersistentDataRows(const PersistentAppData& data);
}
