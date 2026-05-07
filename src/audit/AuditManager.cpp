#include "AuditManager.h"

namespace aegis
{
    namespace
    {
        InfoItem Row(const std::string& name, const std::string& label, const std::string& value, const std::string& detail, const std::string& state = "Ready")
        {
            InfoItem item;
            item.name = name;
            item.label = label;
            item.value = value;
            item.detail = detail;
            item.state = state;
            item.tag = "audit";
            return item;
        }
    }

    AuditRecordResult RecordManagedAuditEvent(const AuditEvent& event)
    {
        AppendAuditEvent(event);
        AuditRecordResult result;
        result.recorded = true;
        result.action = event.action;
        result.detail = event.detail;
        result.hash_preview = AuditFingerprint(event.actor + event.action + event.symbol + event.detail);
        return result;
    }

    std::vector<InfoItem> BuildAuditComplianceRows(const Config& config)
    {
        std::vector<InfoItem> rows = BuildComplianceGuardRows(config);
        const std::vector<InfoItem> trail = BuildAuditTrailRows(4);
        rows.insert(rows.end(), trail.begin(), trail.end());
        rows.push_back(Row("Tamper resistance", "Lightweight", "fingerprint", "Audit rows expose a deterministic fingerprint preview. Full hash chaining can be added when SQLite migration lands."));
        rows.push_back(Row("Coverage target", "Important actions", "settings/providers/paper/reports", "Settings changes, provider failures, paper executions, alerts, exports, migrations, and manual overrides belong in the audit stream."));
        return rows;
    }
}
