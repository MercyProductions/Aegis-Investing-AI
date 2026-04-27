#pragma once

#include <string>
#include <vector>

namespace aegis
{
    std::vector<std::string> SplitCsvLine(const std::string& line);
    std::string CsvCell(const std::string& value);
}
