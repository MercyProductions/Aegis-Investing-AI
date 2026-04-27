#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace aegis
{
    struct DiagnosticEvent
    {
        std::string severity = "info";
        std::string provider = "app";
        std::string endpoint;
        std::string symbol;
        std::string status;
        std::string detail;
        std::string error;
        int http_status = 0;
        long long duration_ms = 0;
        bool used_cache = false;
    };

    struct AuditEvent
    {
        std::string actor = "local";
        std::string action;
        std::string symbol;
        std::string detail;
    };

    std::string RedactSecrets(std::string value);
    void AppendDiagnosticEvent(const DiagnosticEvent& event);
    void AppendAuditEvent(const AuditEvent& event);
    std::vector<std::string> LoadRecentDiagnosticLines(size_t max_lines);
    std::vector<std::string> LoadRecentAuditLines(size_t max_lines);
    bool ExportDiagnosticsSnapshot(const std::filesystem::path& destination, std::string& status);
}
