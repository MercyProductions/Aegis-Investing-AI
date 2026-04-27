#pragma once

#include <string>
#include <vector>

namespace aegis
{
    struct SymbolValidationResult
    {
        bool ok = false;
        std::string symbol;
        std::string reason;
    };

    std::vector<std::string> SplitRawSymbolTokens(const std::string& value);
    SymbolValidationResult ValidateTickerSymbol(const std::string& value);
    std::string NormalizeTickerSymbol(const std::string& value);
}
