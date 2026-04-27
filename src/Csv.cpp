#include "Csv.h"

namespace aegis
{
    std::vector<std::string> SplitCsvLine(const std::string& line)
    {
        std::vector<std::string> cells;
        std::string cell;
        bool quoted = false;

        for (size_t i = 0; i < line.size(); ++i)
        {
            const char c = line[i];
            if (c == '"')
            {
                if (quoted && i + 1 < line.size() && line[i + 1] == '"')
                {
                    cell.push_back('"');
                    ++i;
                }
                else
                {
                    quoted = !quoted;
                }
            }
            else if (c == ',' && !quoted)
            {
                cells.push_back(cell);
                cell.clear();
            }
            else
            {
                cell.push_back(c);
            }
        }

        cells.push_back(cell);
        return cells;
    }

    std::string CsvCell(const std::string& value)
    {
        std::string escaped = value;
        size_t pos = 0;
        while ((pos = escaped.find('"', pos)) != std::string::npos)
        {
            escaped.insert(pos, 1, '"');
            pos += 2;
        }
        return "\"" + escaped + "\"";
    }
}
