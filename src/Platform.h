#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace aegis
{
    struct HttpResponse
    {
        int status_code = 0;
        std::string body;
        std::string raw_headers;
        std::vector<std::string> set_cookies;
        std::string error;
    };

    struct Config
    {
        std::string auth_base_url = "http://127.0.0.1:8000";
        std::string login_path = "/api/auth/login.php";
        std::string stocks_endpoint = "/api/markets-live.php";
        std::string website_base_url = "http://127.0.0.1:5176";
        std::string website_path = "/";
        std::string alpha_vantage_api_key;
        std::string watchlist = "AAPL,MSFT,NVDA,SPY,QQQ,TSLA,AMD,GOOGL";
        int refresh_seconds = 120;
        int max_symbols = 12;
        int model_count = 10;
        double portfolio_cash = 10000.0;
        double max_position_amount = 1500.0;
        double max_portfolio_risk_percent = 2.0;
        int min_conviction = 62;
        bool paper_only_mode = true;
        bool require_manual_confirmation = true;
        bool notifications_enabled = true;
        bool remember_credentials = true;
        bool ui_light_theme = false;
        bool ui_compact_mode = false;
        bool ui_high_contrast = false;
        bool force_live_refresh = false;
        int font_scale_percent = 100;
        int alpha_quote_ttl_seconds = 60;
        int history_cache_hours = 24;
        int research_cache_hours = 12;
        int max_cache_mb = 250;
        std::string sec_user_agent = "Auralith/1.0 (research-only market cockpit; contact: gabriel@local.invalid)";
    };

    struct RememberedCredentials
    {
        bool ok = false;
        std::string username;
        std::string password;
    };

    std::filesystem::path ExecutableDirectory();
    std::filesystem::path AppDataDirectory();
    Config LoadConfig();
    bool SaveConfig(const Config& config);

    std::wstring Utf8ToWide(const std::string& value);
    std::string WideToUtf8(const std::wstring& value);
    std::string GetEnvUtf8(const wchar_t* name);
    std::string JoinUrl(const std::string& base, const std::string& path);
    std::string EscapeJson(const std::string& value);
    std::string Trim(const std::string& value);
    std::string Lower(std::string value);
    std::string Upper(std::string value);
    std::string Initials(const std::string& value);
    std::string NowTimeLabel();

    HttpResponse HttpGet(const std::string& url, const std::string& cookie_header = "");
    HttpResponse HttpPostJson(const std::string& url, const std::string& body, const std::string& cookie_header = "");
    HttpResponse HttpPostForm(const std::string& url, const std::string& body, const std::string& cookie_header = "");
    void SetHttpUserAgent(const std::string& user_agent);
    std::string CookieHeaderFromSetCookies(const std::vector<std::string>& set_cookies);
    std::string HeaderValue(const std::string& raw_headers, const std::string& key);
    std::string UrlEncode(const std::string& value);

    bool SaveRememberedCredentials(const std::string& username, const std::string& password);
    RememberedCredentials LoadRememberedCredentials();
    void DeleteRememberedCredentials();
    bool SaveAlphaVantageApiKey(const std::string& key);
    std::string LoadAlphaVantageApiKey();
    void DeleteAlphaVantageApiKey();
    void AppendDiagnosticLine(const std::string& line);

    void OpenExternalUrl(const std::string& url);
}
