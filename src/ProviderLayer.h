#pragma once

#include "AdvancedAnalytics.h"
#include "StockData.h"

#include <cstddef>
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

    struct ProviderRequest
    {
        std::string provider;
        std::string endpoint;
        std::string symbol;
        int cache_ttl_seconds = 60;
        int timeout_ms = 10000;
        int cancellation_id = 0;
        int attempt = 1;
        int max_attempts = 2;
        int retry_backoff_ms = 1000;
        int http_status = 0;
        bool force_live = false;
        bool cache_exists = false;
        bool cache_fresh = false;
        bool live_attempted = false;
        bool live_success = false;
        bool rate_limited = false;
        std::string response_body;
        std::string error;
    };

    struct ProviderResponseMetadata
    {
        bool should_fetch_live = false;
        bool use_cache = false;
        bool live = false;
        bool stale = false;
        bool fallback = false;
        bool rate_limited = false;
        bool retry = false;
        int next_retry_ms = 0;
        std::string source;
        std::string cache_age_label;
        std::string fallback_reason;
        std::string status;
    };

    std::string SecCompliantUserAgent(const Config& config);
    bool IsProviderRateLimitResponse(int http_status, const std::string& body);
    int NextProviderRetryDelayMs(int attempt, int base_backoff_ms);
    int SecThrottleDelayMs(int elapsed_since_last_ms, int min_interval_ms = 120);
    ProviderResponseMetadata EvaluateProviderRequest(const ProviderRequest& request);
    std::vector<InfoItem> BuildProviderCapabilityRows(const Config& config);
    std::vector<InfoItem> BuildProviderControlRows(const Config& config);
    std::vector<InfoItem> BuildCachePolicyRows(const Config& config);
    std::vector<InfoItem> BuildProviderDiagnosticRows(size_t max_lines);
    std::vector<InfoItem> BuildProviderLimitRows(size_t max_lines);
    std::vector<InfoItem> BuildStorageMigrationRows();
    std::vector<InfoItem> BuildBrokerProfileRows();
    std::vector<InfoItem> BuildProductionReadinessRows(const Config& config);
    StrategyBacktestResult RunStrategyRuleBacktest(const std::vector<Candle>& candles, const std::string& expression, double fee_bps, double slippage_bps);
    bool PruneProviderCaches(int older_than_days, std::string& status);
    BackupValidationResult ValidateBackupDirectory(const std::filesystem::path& directory);
    bool ResetDemoCaches(std::string& status);
}
