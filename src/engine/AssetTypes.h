#pragma once

#include <string>

namespace aegis
{
    enum class AssetClass
    {
        Equity,
        Etf,
        Crypto,
        Forex,
        Commodity,
        Option,
        Unknown
    };

    struct MarketSymbol
    {
        std::string symbol;
        AssetClass asset_class = AssetClass::Equity;
        std::string venue;
        std::string currency = "USD";
    };

    inline const char* AssetClassName(AssetClass asset_class)
    {
        switch (asset_class)
        {
        case AssetClass::Equity: return "Equity";
        case AssetClass::Etf: return "ETF";
        case AssetClass::Crypto: return "Crypto";
        case AssetClass::Forex: return "Forex";
        case AssetClass::Commodity: return "Commodity";
        case AssetClass::Option: return "Option";
        default: return "Unknown";
        }
    }
}
