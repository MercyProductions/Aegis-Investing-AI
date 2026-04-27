#pragma once

#include "AdvancedAnalytics.h"
#include "StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct HoldingImportResult
    {
        bool ok = false;
        std::string error;
        PortfolioHolding row;
    };

    struct AlertImportResult
    {
        bool ok = false;
        std::string error;
        PriceAlert row;
    };

    struct TradePlanImportResult
    {
        bool ok = false;
        std::string error;
        TradePlan row;
    };

    struct SymbolNoteImportResult
    {
        bool ok = false;
        std::string error;
        SymbolNote row;
    };

    struct JournalImportResult
    {
        bool ok = false;
        std::string error;
        JournalEntry row;
    };

    HoldingImportResult ParseHoldingImportRow(const std::vector<std::string>& cells);
    AlertImportResult ParseAlertImportRow(const std::vector<std::string>& cells);
    TradePlanImportResult ParseTradePlanImportRow(const std::vector<std::string>& cells);
    SymbolNoteImportResult ParseSymbolNoteImportRow(const std::vector<std::string>& cells);
    JournalImportResult ParseJournalImportRow(const std::vector<std::string>& cells);
}
