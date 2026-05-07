#include "Platform.h"

#include "Diagnostics.h"

#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace aegis
{
    namespace
    {
        std::filesystem::path RememberFile()
        {
            return AppDataDirectory() / "remember.dat";
        }

        std::filesystem::path AlphaKeyFile()
        {
            return AppDataDirectory() / "alpha-vantage-key.dat";
        }

        std::mutex& UserAgentMutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        std::string& UserAgentStorage()
        {
            static std::string user_agent = "Auralith/1.0 (research-only market cockpit; contact: gabriel@local.invalid)";
            return user_agent;
        }

        std::string SanitizeHeaderValue(std::string value)
        {
            value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
            value.erase(std::remove(value.begin(), value.end(), '\n'), value.end());
            value = Trim(value);
            return value.empty() ? "Auralith/1.0 (research-only market cockpit; contact: gabriel@local.invalid)" : value;
        }

        std::string CurrentHttpUserAgent()
        {
            const std::string env_agent = Trim(GetEnvUtf8(L"AEGIS_SEC_USER_AGENT"));
            if (!env_agent.empty())
                return SanitizeHeaderValue(env_agent);

            std::lock_guard<std::mutex> lock(UserAgentMutex());
            return SanitizeHeaderValue(UserAgentStorage());
        }

        std::string HexEncode(const std::vector<unsigned char>& bytes)
        {
            std::ostringstream stream;
            stream << std::hex << std::setfill('0');
            for (const unsigned char b : bytes)
                stream << std::setw(2) << static_cast<int>(b);
            return stream.str();
        }

        std::vector<unsigned char> HexDecode(const std::string& text)
        {
            std::vector<unsigned char> bytes;
            if (text.size() % 2 != 0)
                return bytes;
            bytes.reserve(text.size() / 2);
            for (size_t i = 0; i < text.size(); i += 2)
            {
                const std::string part = text.substr(i, 2);
                char* end = nullptr;
                const long value = std::strtol(part.c_str(), &end, 16);
                if (end == nullptr || *end != '\0' || value < 0 || value > 255)
                    return {};
                bytes.push_back(static_cast<unsigned char>(value));
            }
            return bytes;
        }

        std::vector<unsigned char> ProtectString(const std::string& value)
        {
            DATA_BLOB in{};
            in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(value.data()));
            in.cbData = static_cast<DWORD>(value.size());

            DATA_BLOB out{};
            if (!CryptProtectData(&in, L"Auralith", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
                return {};

            std::vector<unsigned char> bytes(out.pbData, out.pbData + out.cbData);
            LocalFree(out.pbData);
            return bytes;
        }

        std::string UnprotectString(const std::vector<unsigned char>& bytes)
        {
            if (bytes.empty())
                return {};

            DATA_BLOB in{};
            in.pbData = const_cast<BYTE*>(bytes.data());
            in.cbData = static_cast<DWORD>(bytes.size());
            DATA_BLOB out{};
            if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
                return {};

            std::string value(reinterpret_cast<char*>(out.pbData), reinterpret_cast<char*>(out.pbData) + out.cbData);
            LocalFree(out.pbData);
            return value;
        }

        std::vector<std::string> SplitLines(const std::string& value)
        {
            std::vector<std::string> lines;
            std::stringstream stream(value);
            std::string line;
            while (std::getline(stream, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                lines.push_back(line);
            }
            return lines;
        }

        HttpResponse SendWinHttpRequest(const std::wstring& method, const std::string& url, const std::string* body, const std::string& cookie_header, const std::wstring& content_type = L"application/json")
        {
            HttpResponse response;
            const std::wstring wide_url = Utf8ToWide(url);

            URL_COMPONENTS components{};
            components.dwStructSize = sizeof(components);
            components.dwSchemeLength = static_cast<DWORD>(-1);
            components.dwHostNameLength = static_cast<DWORD>(-1);
            components.dwUrlPathLength = static_cast<DWORD>(-1);
            components.dwExtraInfoLength = static_cast<DWORD>(-1);

            if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &components))
            {
                response.error = "Failed to parse URL.";
                return response;
            }

            const std::wstring host(components.lpszHostName, components.dwHostNameLength);
            std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
            if (components.dwExtraInfoLength > 0)
                path.append(components.lpszExtraInfo, components.dwExtraInfoLength);

            const std::string user_agent = CurrentHttpUserAgent();
            const std::wstring wide_user_agent = Utf8ToWide(user_agent);
            HINTERNET session = WinHttpOpen(wide_user_agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!session)
            {
                response.error = "Failed to open HTTP session.";
                return response;
            }

            WinHttpSetTimeouts(session, 5000, 6000, 9000, 15000);

            HINTERNET connection = WinHttpConnect(session, host.c_str(), components.nPort, 0);
            if (!connection)
            {
                response.error = "Failed to connect to HTTP host.";
                WinHttpCloseHandle(session);
                return response;
            }

            const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET request = WinHttpOpenRequest(connection, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (!request)
            {
                response.error = "Failed to create HTTP request.";
                WinHttpCloseHandle(connection);
                WinHttpCloseHandle(session);
                return response;
            }

            std::wstring headers = L"Accept: application/json,text/csv,*/*\r\n";
            headers += L"User-Agent: ";
            headers += wide_user_agent;
            headers += L"\r\n";
            if (body != nullptr)
            {
                headers += L"Content-Type: ";
                headers += content_type;
                headers += L"\r\n";
            }
            if (!cookie_header.empty())
            {
                headers += L"Cookie: ";
                headers += Utf8ToWide(cookie_header);
                headers += L"\r\n";
            }

            const void* body_data = body == nullptr || body->empty() ? WINHTTP_NO_REQUEST_DATA : body->data();
            const DWORD body_size = body == nullptr ? 0u : static_cast<DWORD>(body->size());
            const BOOL sent = WinHttpSendRequest(
                request,
                headers.c_str(),
                static_cast<DWORD>(-1L),
                const_cast<void*>(body_data),
                body_size,
                body_size,
                0);

            if (!sent || !WinHttpReceiveResponse(request, nullptr))
            {
                response.error = "HTTP request failed.";
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connection);
                WinHttpCloseHandle(session);
                return response;
            }

            DWORD status_code = 0;
            DWORD status_size = sizeof(status_code);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);
            response.status_code = static_cast<int>(status_code);

            DWORD raw_size = 0;
            WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &raw_size, WINHTTP_NO_HEADER_INDEX);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && raw_size > 0)
            {
                std::wstring raw(raw_size / sizeof(wchar_t), L'\0');
                if (WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &raw_size, WINHTTP_NO_HEADER_INDEX))
                {
                    raw.resize(raw_size / sizeof(wchar_t));
                    response.raw_headers = WideToUtf8(raw);
                    for (const std::string& line : SplitLines(response.raw_headers))
                    {
                        const std::string lowered = Lower(line);
                        if (lowered.rfind("set-cookie:", 0) == 0)
                            response.set_cookies.push_back(Trim(line.substr(11)));
                    }
                }
            }

            DWORD available = 0;
            while (WinHttpQueryDataAvailable(request, &available) && available > 0)
            {
                std::string chunk(static_cast<size_t>(available), '\0');
                DWORD downloaded = 0;
                if (!WinHttpReadData(request, chunk.data(), available, &downloaded) || downloaded == 0)
                    break;
                chunk.resize(downloaded);
                response.body += chunk;
            }

            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return response;
        }
    }

    std::filesystem::path ExecutableDirectory()
    {
        wchar_t buffer[MAX_PATH]{};
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return std::filesystem::path(buffer).parent_path();
    }

    std::filesystem::path AppDataDirectory()
    {
        PWSTR raw = nullptr;
        std::filesystem::path base;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw)) && raw != nullptr)
        {
            base = raw;
            CoTaskMemFree(raw);
        }
        else
        {
            base = ExecutableDirectory();
        }

        std::filesystem::path path = base / "Aegis" / "Stock Betting AI";
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        return path;
    }

    Config LoadConfig()
    {
        Config config;
        const std::filesystem::path path = ExecutableDirectory() / "AegisStockBettingAI.config.ini";
        std::ifstream file(path);
        if (!file)
        {
            const std::filesystem::path dev_path = std::filesystem::current_path() / "AegisStockBettingAI.config.ini";
            file.open(dev_path);
        }
        if (!file)
        {
            config.alpha_vantage_api_key = LoadAlphaVantageApiKey();
            return config;
        }

        std::string line;
        while (std::getline(file, line))
        {
            line = Trim(line);
            if (line.empty() || line[0] == '[' || line[0] == '#' || line[0] == ';')
                continue;

            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            const std::string key = Lower(Trim(line.substr(0, eq)));
            const std::string value = Trim(line.substr(eq + 1));
            if (key == "auth_base_url")
                config.auth_base_url = value;
            else if (key == "login_path")
                config.login_path = value;
            else if (key == "stocks_endpoint")
                config.stocks_endpoint = value;
            else if (key == "website_base_url")
                config.website_base_url = value;
            else if (key == "website_path")
                config.website_path = value;
            else if (key == "alpha_vantage_api_key")
                config.alpha_vantage_api_key = value;
            else if (key == "watchlist")
                config.watchlist = value;
            else if (key == "refresh_seconds")
                config.refresh_seconds = std::max(30, std::atoi(value.c_str()));
            else if (key == "max_symbols")
                config.max_symbols = std::clamp(std::atoi(value.c_str()), 1, 50);
            else if (key == "model_count")
                config.model_count = std::clamp(std::atoi(value.c_str()), 2, 32);
            else if (key == "portfolio_cash")
                config.portfolio_cash = std::clamp(std::atof(value.c_str()), 0.0, 1000000000.0);
            else if (key == "max_position_amount")
                config.max_position_amount = std::clamp(std::atof(value.c_str()), 1.0, 100000000.0);
            else if (key == "max_portfolio_risk_percent")
                config.max_portfolio_risk_percent = std::clamp(std::atof(value.c_str()), 0.1, 25.0);
            else if (key == "min_conviction")
                config.min_conviction = std::clamp(std::atoi(value.c_str()), 1, 99);
            else if (key == "paper_only_mode")
                config.paper_only_mode = Lower(value) != "false" && value != "0";
            else if (key == "require_manual_confirmation")
                config.require_manual_confirmation = Lower(value) != "false" && value != "0";
            else if (key == "notifications_enabled")
                config.notifications_enabled = Lower(value) != "false" && value != "0";
            else if (key == "remember_credentials")
                config.remember_credentials = Lower(value) != "false" && value != "0";
            else if (key == "ui_light_theme")
                config.ui_light_theme = Lower(value) == "true" || value == "1";
            else if (key == "ui_compact_mode")
                config.ui_compact_mode = Lower(value) == "true" || value == "1";
            else if (key == "ui_high_contrast")
                config.ui_high_contrast = Lower(value) == "true" || value == "1";
            else if (key == "font_scale_percent")
                config.font_scale_percent = std::clamp(std::atoi(value.c_str()), 85, 150);
            else if (key == "force_live_refresh")
                config.force_live_refresh = Lower(value) == "true" || value == "1";
            else if (key == "alpha_quote_ttl_seconds")
                config.alpha_quote_ttl_seconds = std::clamp(std::atoi(value.c_str()), 15, 900);
            else if (key == "history_cache_hours")
                config.history_cache_hours = std::clamp(std::atoi(value.c_str()), 1, 720);
            else if (key == "research_cache_hours")
                config.research_cache_hours = std::clamp(std::atoi(value.c_str()), 1, 720);
            else if (key == "max_cache_mb")
                config.max_cache_mb = std::clamp(std::atoi(value.c_str()), 25, 2048);
            else if (key == "sec_user_agent")
                config.sec_user_agent = value;
        }

        const std::string secure_key = LoadAlphaVantageApiKey();
        if (!secure_key.empty())
            config.alpha_vantage_api_key = secure_key;
        else if (!Trim(config.alpha_vantage_api_key).empty())
            SaveAlphaVantageApiKey(config.alpha_vantage_api_key);

        SetHttpUserAgent(config.sec_user_agent);

        return config;
    }

    bool SaveConfig(const Config& config)
    {
        const std::filesystem::path path = ExecutableDirectory() / "AegisStockBettingAI.config.ini";
        std::ofstream file(path, std::ios::trunc);
        if (!file)
            return false;

        if (!Trim(config.alpha_vantage_api_key).empty())
            SaveAlphaVantageApiKey(config.alpha_vantage_api_key);
        else
            DeleteAlphaVantageApiKey();

        file << "[auth]\n";
        file << "auth_base_url=" << config.auth_base_url << "\n";
        file << "login_path=" << config.login_path << "\n";
        file << "stocks_endpoint=" << config.stocks_endpoint << "\n";
        file << "website_base_url=" << config.website_base_url << "\n";
        file << "website_path=" << config.website_path << "\n\n";

        file << "[data]\n";
        file << "alpha_vantage_api_key=\n";
        file << "watchlist=" << config.watchlist << "\n";
        file << "max_symbols=" << std::clamp(config.max_symbols, 1, 50) << "\n";
        file << "model_count=" << std::clamp(config.model_count, 2, 32) << "\n\n";

        file << "[app]\n";
        file << "refresh_seconds=" << std::max(30, config.refresh_seconds) << "\n";
        file << "notifications_enabled=" << (config.notifications_enabled ? "true" : "false") << "\n";
        file << "remember_credentials=" << (config.remember_credentials ? "true" : "false") << "\n";
        file << "ui_light_theme=" << (config.ui_light_theme ? "true" : "false") << "\n";
        file << "ui_compact_mode=" << (config.ui_compact_mode ? "true" : "false") << "\n";
        file << "ui_high_contrast=" << (config.ui_high_contrast ? "true" : "false") << "\n";
        file << "font_scale_percent=" << std::clamp(config.font_scale_percent, 85, 150) << "\n";
        file << "\n[providers]\n";
        file << "sec_user_agent=" << SanitizeHeaderValue(config.sec_user_agent) << "\n";
        file << "force_live_refresh=" << (config.force_live_refresh ? "true" : "false") << "\n";
        file << "alpha_quote_ttl_seconds=" << std::clamp(config.alpha_quote_ttl_seconds, 15, 900) << "\n";
        file << "history_cache_hours=" << std::clamp(config.history_cache_hours, 1, 720) << "\n";
        file << "research_cache_hours=" << std::clamp(config.research_cache_hours, 1, 720) << "\n";
        file << "max_cache_mb=" << std::clamp(config.max_cache_mb, 25, 2048) << "\n";
        file << "\n[risk]\n";
        file << "portfolio_cash=" << std::clamp(config.portfolio_cash, 0.0, 1000000000.0) << "\n";
        file << "max_position_amount=" << std::clamp(config.max_position_amount, 1.0, 100000000.0) << "\n";
        file << "max_portfolio_risk_percent=" << std::clamp(config.max_portfolio_risk_percent, 0.1, 25.0) << "\n";
        file << "min_conviction=" << std::clamp(config.min_conviction, 1, 99) << "\n";
        file << "paper_only_mode=" << (config.paper_only_mode ? "true" : "false") << "\n";
        file << "require_manual_confirmation=" << (config.require_manual_confirmation ? "true" : "false") << "\n";
        SetHttpUserAgent(config.sec_user_agent);
        return true;
    }

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
            return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring wide(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), size);
        return wide;
    }

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty())
            return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), utf8.data(), size, nullptr, nullptr);
        return utf8;
    }

    std::string GetEnvUtf8(const wchar_t* name)
    {
        wchar_t buffer[4096]{};
        const DWORD read = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
        if (read == 0 || read >= std::size(buffer))
            return {};
        return WideToUtf8(buffer);
    }

    std::string JoinUrl(const std::string& base, const std::string& path)
    {
        if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0)
            return path;
        if (base.empty())
            return path;
        const bool base_slash = base.back() == '/';
        const bool path_slash = !path.empty() && path.front() == '/';
        if (base_slash && path_slash)
            return base.substr(0, base.size() - 1) + path;
        if (!base_slash && !path_slash)
            return base + "/" + path;
        return base + path;
    }

    std::string EscapeJson(const std::string& value)
    {
        std::string out;
        out.reserve(value.size() + 8);
        for (const char c : value)
        {
            switch (c)
            {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                out += static_cast<unsigned char>(c) < 0x20 ? ' ' : c;
                break;
            }
        }
        return out;
    }

    std::string Trim(const std::string& value)
    {
        size_t first = 0;
        while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])) != 0)
            ++first;
        size_t last = value.size();
        while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
            --last;
        return value.substr(first, last - first);
    }

    std::string Lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    std::string Upper(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return value;
    }

    std::string Initials(const std::string& value)
    {
        std::string out;
        bool take_next = true;
        for (const char c : value)
        {
            if (std::isalnum(static_cast<unsigned char>(c)) == 0)
            {
                take_next = true;
                continue;
            }
            if (take_next || out.empty())
            {
                out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                take_next = false;
                if (out.size() == 2)
                    break;
            }
        }
        return out.empty() ? "AI" : out;
    }

    std::string NowTimeLabel()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm local{};
        localtime_s(&local, &tt);
        std::ostringstream stream;
        stream << std::put_time(&local, "%H:%M");
        return stream.str();
    }

    HttpResponse HttpGet(const std::string& url, const std::string& cookie_header)
    {
        return SendWinHttpRequest(L"GET", url, nullptr, cookie_header);
    }

    HttpResponse HttpPostJson(const std::string& url, const std::string& body, const std::string& cookie_header)
    {
        return SendWinHttpRequest(L"POST", url, &body, cookie_header);
    }

    HttpResponse HttpPostForm(const std::string& url, const std::string& body, const std::string& cookie_header)
    {
        return SendWinHttpRequest(L"POST", url, &body, cookie_header, L"application/x-www-form-urlencoded");
    }

    void SetHttpUserAgent(const std::string& user_agent)
    {
        std::lock_guard<std::mutex> lock(UserAgentMutex());
        UserAgentStorage() = SanitizeHeaderValue(user_agent);
    }

    std::string CookieHeaderFromSetCookies(const std::vector<std::string>& set_cookies)
    {
        std::vector<std::string> parts;
        for (const std::string& cookie : set_cookies)
        {
            const size_t end = cookie.find(';');
            std::string first = Trim(cookie.substr(0, end));
            if (first.empty() || first.find('=') == std::string::npos)
                continue;
            parts.push_back(first);
        }
        std::ostringstream header;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            if (i > 0)
                header << "; ";
            header << parts[i];
        }
        return header.str();
    }

    std::string HeaderValue(const std::string& raw_headers, const std::string& key)
    {
        const std::string target = Lower(key) + ":";
        for (const std::string& line : SplitLines(raw_headers))
        {
            const std::string trimmed = Trim(line);
            if (Lower(trimmed).rfind(target, 0) == 0)
                return Trim(trimmed.substr(target.size()));
        }
        return {};
    }

    std::string UrlEncode(const std::string& value)
    {
        std::ostringstream stream;
        stream << std::hex << std::uppercase;
        for (const unsigned char c : value)
        {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
                stream << static_cast<char>(c);
            else if (c == ' ')
                stream << '+';
            else
                stream << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
        return stream.str();
    }

    bool SaveRememberedCredentials(const std::string& username, const std::string& password)
    {
        std::error_code ec;
        std::filesystem::create_directories(AppDataDirectory(), ec);
        const std::vector<unsigned char> protected_password = ProtectString(password);
        if (protected_password.empty())
            return false;

        std::ofstream file(RememberFile(), std::ios::trunc);
        if (!file)
            return false;
        file << username << "\n" << HexEncode(protected_password) << "\n";
        return true;
    }

    RememberedCredentials LoadRememberedCredentials()
    {
        RememberedCredentials creds;
        std::ifstream file(RememberFile());
        if (!file)
            return creds;
        std::string username;
        std::string encrypted;
        std::getline(file, username);
        std::getline(file, encrypted);
        const std::string password = UnprotectString(HexDecode(Trim(encrypted)));
        if (Trim(username).empty() || password.empty())
            return creds;
        creds.ok = true;
        creds.username = Trim(username);
        creds.password = password;
        return creds;
    }

    void DeleteRememberedCredentials()
    {
        std::error_code ec;
        std::filesystem::remove(RememberFile(), ec);
    }

    bool SaveAlphaVantageApiKey(const std::string& key)
    {
        const std::string trimmed = Trim(key);
        if (trimmed.empty())
        {
            DeleteAlphaVantageApiKey();
            return true;
        }

        std::error_code ec;
        std::filesystem::create_directories(AppDataDirectory(), ec);
        const std::vector<unsigned char> protected_key = ProtectString(trimmed);
        if (protected_key.empty())
            return false;

        std::ofstream file(AlphaKeyFile(), std::ios::trunc);
        if (!file)
            return false;
        file << HexEncode(protected_key) << "\n";
        return true;
    }

    std::string LoadAlphaVantageApiKey()
    {
        std::ifstream file(AlphaKeyFile());
        if (!file)
            return {};
        std::string encrypted;
        std::getline(file, encrypted);
        return UnprotectString(HexDecode(Trim(encrypted)));
    }

    void DeleteAlphaVantageApiKey()
    {
        std::error_code ec;
        std::filesystem::remove(AlphaKeyFile(), ec);
    }

    void AppendDiagnosticLine(const std::string& line)
    {
        AppendDiagnosticEvent({ "info", "legacy", "", "", "ok", line, "", 0, 0, false });
    }

    void OpenExternalUrl(const std::string& url)
    {
        ShellExecuteW(nullptr, L"open", Utf8ToWide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}
