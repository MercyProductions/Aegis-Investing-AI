#include "SymbolRules.h"

#include "Platform.h"

#include <cctype>

namespace aegis
{
    namespace
    {
        bool IsTickerPunctuation(char c)
        {
            return c == '.' || c == '-';
        }
    }

    std::vector<std::string> SplitRawSymbolTokens(const std::string& value)
    {
        std::vector<std::string> tokens;
        std::string token;
        for (char c : value)
        {
            if (c == ',' || c == ';' || c == '\n' || c == '\r' || c == '\t')
            {
                if (!Trim(token).empty())
                    tokens.push_back(Trim(token));
                token.clear();
            }
            else
            {
                token.push_back(c);
            }
        }
        if (!Trim(token).empty())
            tokens.push_back(Trim(token));
        return tokens;
    }

    SymbolValidationResult ValidateTickerSymbol(const std::string& value)
    {
        const std::string trimmed = Trim(value);
        SymbolValidationResult result;
        if (trimmed.empty())
        {
            result.reason = "Symbol is empty.";
            return result;
        }

        bool has_alpha = false;
        bool invalid = false;
        std::string normalized;
        for (size_t i = 0; i < trimmed.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(trimmed[i]);
            if (i == 0 && c == '$')
                continue;
            if (std::isspace(c))
                continue;
            if (std::isalnum(c))
            {
                normalized.push_back(static_cast<char>(std::toupper(c)));
                if (std::isalpha(c))
                    has_alpha = true;
            }
            else if (c == '/')
            {
                normalized.push_back('.');
            }
            else if (IsTickerPunctuation(static_cast<char>(c)))
            {
                normalized.push_back(static_cast<char>(c));
            }
            else
            {
                invalid = true;
            }
        }

        result.symbol = normalized;
        if (invalid)
        {
            result.reason = "Symbol contains unsupported characters.";
            return result;
        }
        if (normalized.empty())
        {
            result.reason = "Symbol is empty after cleanup.";
            return result;
        }
        if (!has_alpha)
        {
            result.reason = "Symbol must contain at least one letter.";
            return result;
        }
        if (normalized.size() > 12)
        {
            result.reason = "Symbol is longer than 12 characters.";
            return result;
        }
        if (!std::isalnum(static_cast<unsigned char>(normalized.front())) || !std::isalnum(static_cast<unsigned char>(normalized.back())))
        {
            result.reason = "Symbol must start and end with a letter or number.";
            return result;
        }
        if (normalized.find("..") != std::string::npos ||
            normalized.find("--") != std::string::npos ||
            normalized.find(".-") != std::string::npos ||
            normalized.find("-.") != std::string::npos)
        {
            result.reason = "Symbol has invalid punctuation placement.";
            return result;
        }

        result.ok = true;
        result.reason = normalized == trimmed ? "Valid symbol." : "Normalized symbol.";
        return result;
    }

    std::string NormalizeTickerSymbol(const std::string& value)
    {
        const SymbolValidationResult result = ValidateTickerSymbol(value);
        return result.ok ? result.symbol : std::string{};
    }
}
