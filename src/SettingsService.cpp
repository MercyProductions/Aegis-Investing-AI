#include "SettingsService.h"

#include "Platform.h"
#include "ProviderLayer.h"

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
        config.font_scale_percent = std::clamp(config.font_scale_percent, 85, 150);
        config.alpha_quote_ttl_seconds = std::clamp(config.alpha_quote_ttl_seconds, 15, 900);
        config.history_cache_hours = std::clamp(config.history_cache_hours, 1, 720);
        config.research_cache_hours = std::clamp(config.research_cache_hours, 1, 720);
        config.max_cache_mb = std::clamp(config.max_cache_mb, 25, 2048);
        config.ui_light_theme = input.ui_light_theme;
        config.ui_compact_mode = input.ui_compact_mode;
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

    std::vector<InfoItem> BuildSettingsHealthRows(const Config& config)
    {
        const std::vector<std::string> symbols = SplitWatchlist(config.watchlist);
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
            { "Alpha Vantage", "Provider", has_key ? "Configured" : "Missing", "", has_key ? "Direct quote/history/research refresh can use the saved key." : "Demo/cache fallback remains available, but live market data is limited.", "settings", RowState(has_key) },
            { "Auth bridge", "Local/website", has_auth ? config.auth_base_url : "Missing", "", "Used only for optional website bridge login.", "settings", RowState(has_auth) },
            { "SEC user agent", "Compliance", sec_has_contact ? "Contact present" : "Needs contact", "", sec_agent, "settings", RowState(sec_has_contact) },
            { "Cache policy", "Freshness", cache_ok ? "Configured" : "Review", "", "Quote/history/research TTL and max cache size are bounded before save.", "settings", RowState(cache_ok) },
            { "Safety", "Guardrails", safety_ok ? "Paper locked" : "Review", "", "Paper-only mode and manual confirmation should stay enabled for research workflows.", "settings", RowState(safety_ok) }
        };
    }
}
