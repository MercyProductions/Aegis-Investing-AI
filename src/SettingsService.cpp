#include "SettingsService.h"

#include "Platform.h"
#include "ProviderLayer.h"
#include "SymbolRules.h"

#include <algorithm>

namespace aegis
{
    namespace
    {
        constexpr const char* kDefaultAuthBaseUrl = "http://127.0.0.1:8000";

        std::string RowState(bool ok)
        {
            return ok ? "Ready" : "Review";
        }
    }

    Config ApplySettingsInput(Config config, const SettingsInput& input)
    {
        config.watchlist = JoinWatchlist(SplitWatchlist(input.watchlist));
        config.alpha_vantage_api_key = Trim(input.alpha_vantage_api_key);
        config.auth_base_url = Trim(input.auth_base_url);
        config.sec_user_agent = Trim(input.sec_user_agent);

        if (config.auth_base_url.empty())
            config.auth_base_url = kDefaultAuthBaseUrl;
        if (config.sec_user_agent.empty())
            config.sec_user_agent = SecCompliantUserAgent(config);

        config.refresh_seconds = std::clamp(config.refresh_seconds, 30, 900);
        config.max_symbols = std::clamp(config.max_symbols, 1, 50);
        config.model_count = std::clamp(config.model_count, 2, 32);
        config.font_scale_percent = std::clamp(input.font_scale_percent, 85, 150);
        config.alpha_quote_ttl_seconds = std::clamp(config.alpha_quote_ttl_seconds, 15, 900);
        config.history_cache_hours = std::clamp(config.history_cache_hours, 1, 720);
        config.research_cache_hours = std::clamp(config.research_cache_hours, 1, 720);
        config.max_cache_mb = std::clamp(config.max_cache_mb, 25, 2048);
        config.ui_light_theme = input.ui_light_theme;
        config.ui_compact_mode = input.ui_compact_mode;
        config.ui_high_contrast = input.ui_high_contrast;
        return config;
    }

    SecretClearResult ClearStoredSecrets(Config config)
    {
        DeleteRememberedCredentials();
        DeleteAlphaVantageApiKey();
        config.alpha_vantage_api_key.clear();
        SaveConfig(config);
        return { config, "API keys and remembered credentials cleared." };
    }

    WatchlistLimitResult ValidateWatchlistLimits(const Config& config)
    {
        WatchlistLimitResult result;
        result.symbol_count = static_cast<int>(SplitWatchlist(config.watchlist).size());
        result.refresh_cap = std::clamp(config.max_symbols, 1, 50);
        result.within_limit = result.symbol_count <= result.refresh_cap;
        result.summary = std::to_string(result.symbol_count) + " watched / " + std::to_string(result.refresh_cap) + " refreshed";
        if (result.symbol_count == 0)
            result.detail = "Add at least one ticker before relying on refresh workflows.";
        else if (result.within_limit)
            result.detail = "Every watchlist symbol is inside the configured refresh cap.";
        else
            result.detail = "Only the first " + std::to_string(result.refresh_cap) + " normalized symbols refresh per cycle; raise Max symbols or trim the watchlist.";
        return result;
    }

    std::vector<InfoItem> BuildSettingsHealthRows(const Config& config)
    {
        const std::vector<std::string> symbols = SplitWatchlist(config.watchlist);
        const WatchlistLimitResult limits = ValidateWatchlistLimits(config);
        int invalid_tokens = 0;
        for (const std::string& token : SplitRawSymbolTokens(config.watchlist))
        {
            if (!ValidateTickerSymbol(token).ok)
                ++invalid_tokens;
        }
        const bool has_watchlist = !symbols.empty();
        const bool has_key = !Trim(config.alpha_vantage_api_key).empty();
        const bool has_auth = !Trim(config.auth_base_url).empty();
        const std::string sec_agent = SecCompliantUserAgent(config);
        const bool sec_has_contact = sec_agent.find('@') != std::string::npos ||
            sec_agent.find("contact") != std::string::npos ||
            sec_agent.find("Contact") != std::string::npos;
        const bool cache_ok = config.alpha_quote_ttl_seconds >= 15 &&
            config.history_cache_hours >= 1 &&
            config.research_cache_hours >= 1 &&
            config.max_cache_mb >= 25;
        const bool safety_ok = config.paper_only_mode && config.require_manual_confirmation;

        return {
            { "Watchlist", "Input", std::to_string(symbols.size()) + " symbol(s)", "", has_watchlist ? "Normalized and ready for provider refresh." : "Add at least one ticker before relying on refresh workflows.", "settings", RowState(has_watchlist) },
            { "Refresh cap", "Provider", limits.summary, "", limits.detail, "settings", RowState(limits.within_limit) },
            { "Symbol validation", "Input", invalid_tokens == 0 ? "Clean" : std::to_string(invalid_tokens) + " rejected", "", "Accepts US-style tickers with letters, numbers, dot, dash, optional $ prefix, and slash-to-dot normalization.", "settings", RowState(invalid_tokens == 0) },
            { "Alpha Vantage", "Provider", has_key ? "Configured" : "Missing", "", has_key ? "Direct quote/history/research refresh can use the saved key." : "Demo/cache fallback remains available, but live market data is limited.", "settings", RowState(has_key) },
            { "Auth bridge", "Local/website", has_auth ? config.auth_base_url : "Missing", "", "Used only for optional website bridge login.", "settings", RowState(has_auth) },
            { "SEC user agent", "Compliance", sec_has_contact ? "Contact present" : "Needs contact", "", sec_agent, "settings", RowState(sec_has_contact) },
            { "Cache policy", "Freshness", cache_ok ? "Configured" : "Review", "", "Quote/history/research TTL and max cache size are bounded before save.", "settings", RowState(cache_ok) },
            { "Safety", "Guardrails", safety_ok ? "Paper locked" : "Review", "", "Paper-only mode and manual confirmation should stay enabled for research workflows.", "settings", RowState(safety_ok) }
        };
    }
}
