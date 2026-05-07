#pragma once

#include "../Diagnostics.h"
#include "../StockData.h"

#include <string>
#include <vector>

namespace aegis
{
    struct AuditRecordResult
    {
        bool recorded = false;
        std::string action;
        std::string detail;
        std::string hash_preview;
    };

    AuditRecordResult RecordManagedAuditEvent(const AuditEvent& event);
    std::string AuditFingerprint(const std::string& line);
    std::vector<InfoItem> BuildAuditComplianceRows(const Config& config);
    std::vector<InfoItem> BuildAuditTrailRows(size_t max_lines);
    std::vector<InfoItem> BuildComplianceGuardRows(const Config& config);
}
