#include "Diagnostics.h"

#include "Platform.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

namespace aegis
{
    namespace
    {
        std::mutex g_diagnostic_mutex;

        std::string ScrubTokenAfter(std::string value, const std::string& marker, const std::string& delimiters, bool skip_leading_separators = true)
        {
            std::string lowered = Lower(value);
            size_t pos = lowered.find(marker);
            while (pos != std::string::npos)
            {
                size_t value_start = pos + marker.size();
                if (skip_leading_separators)
                {
                    while (value_start < value.size())
                    {
                        const char c = value[value_start];
                        if (c != ' ' && c != '\t' && c != ':' && c != '=')
                            break;
                        ++value_start;
                    }
                }
                size_t end = value.find_first_of(delimiters, value_start);
                if (end == std::string::npos)
                    end = value.size();
                if (end > value_start)
                    value.replace(value_start, end - value_start, "REDACTED");
                lowered = Lower(value);
                pos = lowered.find(marker, value_start + 8);
            }
            return value;
        }

        std::string ScrubQueryToken(std::string value, const std::string& marker)
        {
            return ScrubTokenAfter(std::move(value), marker, "& \t\r\n'\";,)<>");
        }

        std::string ScrubHeaderToken(std::string value, const std::string& marker)
        {
            return ScrubTokenAfter(std::move(value), marker, "\r\n");
        }

        std::string ScrubJsonToken(std::string value, const std::string& marker)
        {
            return ScrubTokenAfter(std::move(value), marker, "\"", false);
        }

        std::filesystem::path DiagnosticsJsonlPath()
        {
            return AppDataDirectory() / "diagnostics.jsonl";
        }

        std::filesystem::path AuditJsonlPath()
        {
            return AppDataDirectory() / "audit-log.jsonl";
        }

        void WriteJsonField(std::ofstream& file, const char* name, const std::string& value, bool comma = true)
        {
            file << "\"" << name << "\":\"" << EscapeJson(RedactSecrets(value)) << "\"";
            if (comma)
                file << ",";
        }

        void WriteLegacyLine(const std::string& file_name, const std::string& line)
        {
            std::ofstream file(AppDataDirectory() / file_name, std::ios::app);
            if (file)
                file << NowTimeLabel() << " " << RedactSecrets(line) << "\n";
        }

        std::vector<std::string> TailLines(const std::filesystem::path& path, size_t max_lines)
        {
            std::ifstream file(path);
            if (!file)
                return {};

            std::vector<std::string> lines;
            std::string line;
            while (std::getline(file, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                lines.push_back(line);
                if (lines.size() > max_lines)
                    lines.erase(lines.begin());
            }
            return lines;
        }
    }

    std::string RedactSecrets(std::string value)
    {
        for (const char* marker : {
            "apikey=",
            "api_key=",
            "alpha_vantage_api_key=",
            "access_token=",
            "refresh_token=",
            "id_token=",
            "token=",
            "password=",
            "authorization=",
            "cookie=",
        })
        {
            value = ScrubQueryToken(std::move(value), marker);
        }
        for (const char* marker : {
            "authorization:",
            "proxy-authorization:",
            "cookie:",
            "set-cookie:",
            "x-api-key:",
            "api-key:",
        })
        {
            value = ScrubHeaderToken(std::move(value), marker);
        }
        for (const char* marker : {
            "\"password\":\"",
            "\"alpha_vantage_api_key\":\"",
            "\"api_key\":\"",
            "\"apikey\":\"",
            "\"access_token\":\"",
            "\"refresh_token\":\"",
            "\"id_token\":\"",
            "\"token\":\"",
            "\"authorization\":\"",
            "\"cookie\":\"",
        })
        {
            value = ScrubJsonToken(std::move(value), marker);
        }
        value = ScrubQueryToken(std::move(value), "bearer ");
        return value;
    }

    void AppendDiagnosticEvent(const DiagnosticEvent& event)
    {
        std::lock_guard<std::mutex> lock(g_diagnostic_mutex);
        std::error_code ec;
        std::filesystem::create_directories(AppDataDirectory(), ec);

        std::ofstream file(DiagnosticsJsonlPath(), std::ios::app);
        if (file)
        {
            file << "{";
            WriteJsonField(file, "timestamp", NowTimeLabel());
            WriteJsonField(file, "severity", event.severity);
            WriteJsonField(file, "provider", event.provider);
            WriteJsonField(file, "endpoint", event.endpoint);
            WriteJsonField(file, "symbol", event.symbol);
            WriteJsonField(file, "status", event.status);
            WriteJsonField(file, "detail", event.detail);
            WriteJsonField(file, "error", event.error);
            file << "\"http_status\":" << event.http_status << ",";
            file << "\"duration_ms\":" << event.duration_ms << ",";
            file << "\"used_cache\":" << (event.used_cache ? "true" : "false");
            file << "}\n";
        }

        std::string legacy = event.provider;
        if (!event.endpoint.empty())
            legacy += " " + event.endpoint;
        if (!event.symbol.empty())
            legacy += " " + event.symbol;
        if (!event.status.empty())
            legacy += " " + event.status;
        if (!event.detail.empty())
            legacy += " " + event.detail;
        if (!event.error.empty())
            legacy += " error=" + event.error;
        WriteLegacyLine("diagnostics.log", legacy);
    }

    void AppendAuditEvent(const AuditEvent& event)
    {
        std::lock_guard<std::mutex> lock(g_diagnostic_mutex);
        std::error_code ec;
        std::filesystem::create_directories(AppDataDirectory(), ec);

        std::ofstream json(AuditJsonlPath(), std::ios::app);
        if (json)
        {
            json << "{";
            WriteJsonField(json, "timestamp", NowTimeLabel());
            WriteJsonField(json, "actor", event.actor);
            WriteJsonField(json, "action", event.action);
            WriteJsonField(json, "symbol", event.symbol);
            WriteJsonField(json, "detail", event.detail, false);
            json << "}\n";
        }

        std::ofstream tsv(AppDataDirectory() / "audit-log.tsv", std::ios::app);
        if (tsv)
            tsv << NowTimeLabel() << '\t' << RedactSecrets(event.action) << '\t' << RedactSecrets(event.symbol) << '\t' << RedactSecrets(event.detail) << '\n';
    }

    std::vector<std::string> LoadRecentDiagnosticLines(size_t max_lines)
    {
        std::vector<std::string> lines = TailLines(DiagnosticsJsonlPath(), max_lines);
        if (lines.empty())
            lines = TailLines(AppDataDirectory() / "diagnostics.log", max_lines);
        return lines;
    }

    std::vector<std::string> LoadRecentAuditLines(size_t max_lines)
    {
        std::vector<std::string> lines = TailLines(AuditJsonlPath(), max_lines);
        if (lines.empty())
            lines = TailLines(AppDataDirectory() / "audit-log.tsv", max_lines);
        return lines;
    }

    bool ExportDiagnosticsSnapshot(const std::filesystem::path& destination, std::string& status)
    {
        std::error_code ec;
        std::filesystem::create_directories(destination, ec);
        if (ec)
        {
            status = "Could not create diagnostics export folder.";
            return false;
        }

        int copied = 0;
        for (const char* name : { "diagnostics.jsonl", "diagnostics.log", "audit-log.jsonl", "audit-log.tsv" })
        {
            const std::filesystem::path from = AppDataDirectory() / name;
            const std::filesystem::path to = destination / name;
            if (std::filesystem::exists(from, ec))
            {
                std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
                if (!ec)
                    ++copied;
            }
        }

        status = "Exported " + std::to_string(copied) + " diagnostic/audit files.";
        return copied > 0;
    }
}
