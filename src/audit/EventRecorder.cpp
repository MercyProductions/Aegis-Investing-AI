#include "AuditManager.h"

#include <iomanip>
#include <sstream>

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
            item.tag = "audit-trail";
            return item;
        }
    }

    std::string AuditFingerprint(const std::string& line)
    {
        unsigned long long hash = 1469598103934665603ull;
        for (unsigned char ch : line)
        {
            hash ^= static_cast<unsigned long long>(ch);
            hash *= 1099511628211ull;
        }
        std::ostringstream out;
        out << std::hex << std::setw(12) << std::setfill('0') << (hash & 0xFFFFFFFFFFFFull);
        return out.str();
    }

    std::vector<InfoItem> BuildAuditTrailRows(size_t max_lines)
    {
        std::vector<InfoItem> rows;
        const std::vector<std::string> lines = LoadRecentAuditLines(max_lines);
        if (lines.empty())
        {
            rows.push_back(Row("Recent audit", "Empty", "--", "No recent audit rows were available.", "Review"));
            return rows;
        }

        for (const std::string& line : lines)
        {
            rows.push_back(Row("Recent audit", "Recorded", AuditFingerprint(line), RedactSecrets(line)));
        }
        return rows;
    }
}
