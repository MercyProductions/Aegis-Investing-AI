#pragma once

#include "AdvancedAnalytics.h"
#include "StockData.h"

#include <filesystem>
#include <string>
#include <vector>

namespace aegis
{
    struct StrategyBacktestResult
    {
        bool ok = false;
        std::string status;
        BacktestResult backtest;
    };

    struct BackupValidationResult
    {
        bool ok = false;
        std::string summary;
        std::vector<std::string> issues;
    };

    std::string SecCompliantUserAgent(const Config& config);
    std::vector<InfoItem> BuildProviderControlRows(const Config& config);
    std::vector<InfoItem> BuildCachePolicyRows(const Config& config);
    std::vector<InfoItem> BuildStorageMigrationRows();
    std::vector<InfoItem> BuildBrokerProfileRows();
    std::vector<InfoItem> BuildProductionReadinessRows(const Config& config);
    StrategyBacktestResult RunStrategyRuleBacktest(const std::vector<Candle>& candles, const std::string& expression, double fee_bps, double slippage_bps);
    bool PruneProviderCaches(int older_than_days, std::string& status);
    BackupValidationResult ValidateBackupDirectory(const std::filesystem::path& directory);
    bool ResetDemoCaches(std::string& status);
}
