#include "SharedModels.h"

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
            item.tag = "shared-model";
            item.source = "Auralith shared contracts";
            return item;
        }

        SharedModelField Field(const std::string& name, const std::string& type, bool required, const std::string& detail)
        {
            return { name, type, required, detail };
        }
    }

    std::string SharedModelContractVersion()
    {
        return "professional-platform.v1";
    }

    std::vector<SharedModelContract> BuildSharedModelContracts()
    {
        return {
            {
                "Symbol",
                SharedModelContractVersion(),
                "Shared",
                "Canonical instrument identity used by desktop, core, web, SDK, and future companion clients.",
                {
                    Field("symbol", "string", true, "Uppercase provider-normalized ticker or asset symbol."),
                    Field("assetClass", "enum", true, "Equity, ETF, Crypto, Forex, Commodity, Option, Index, or Unknown."),
                    Field("name", "string", false, "Display name from provider metadata."),
                    Field("sector", "string", false, "Sector or category label when available."),
                    Field("currency", "string", true, "ISO currency code, usually USD.")
                }
            },
            {
                "Quote",
                SharedModelContractVersion(),
                "Providers",
                "All market prices must carry source and freshness metadata before entering UI or analytics.",
                {
                    Field("symbol", "string", true, "Symbol contract key."),
                    Field("price", "number", true, "Latest known price."),
                    Field("changePercent", "number", false, "Session change in percent form."),
                    Field("provider", "string", true, "Named source or cache layer."),
                    Field("fetchedAt", "timestamp", true, "ISO-like local timestamp when the provider/cache produced the value."),
                    Field("freshness", "enum", true, "live/fresh, delayed, stale, fallback, or failed/error."),
                    Field("delayed", "boolean", true, "True when feed entitlement, venue delay, or cache age reduces intraday confidence."),
                    Field("fallback", "boolean", true, "True when data is demo/cache/fallback rather than provider-fresh."),
                    Field("confidence", "integer", true, "0-100 research confidence after freshness, fallback, provider errors, and estimated metrics."),
                    Field("error", "string", false, "Provider notice, parse failure, rate limit, entitlement issue, or transport error.")
                }
            },
            {
                "Position",
                SharedModelContractVersion(),
                "Portfolio",
                "Paper portfolio position contract with cost basis and valuation state.",
                {
                    Field("portfolioId", "string", true, "Local profile or portfolio id."),
                    Field("symbol", "string", true, "Symbol contract key."),
                    Field("quantity", "number", true, "Position size."),
                    Field("averageCost", "number", true, "Paper cost basis."),
                    Field("lastPrice", "number", false, "Latest marked price with quote metadata nearby.")
                }
            },
            {
                "Alert",
                SharedModelContractVersion(),
                "Alerts",
                "Research alert contract used by desktop, web, core monitoring, notifications, and audit.",
                {
                    Field("id", "string", true, "Stable alert id."),
                    Field("symbol", "string", true, "Symbol contract key."),
                    Field("direction", "enum", true, "above or below."),
                    Field("triggerPrice", "number", true, "Research trigger level."),
                    Field("enabled", "boolean", true, "Whether core should monitor it.")
                }
            },
            {
                "RiskEvent",
                SharedModelContractVersion(),
                "Risk",
                "Risk state emitted by engines, paper execution, portfolio review, and resilience checks.",
                {
                    Field("severity", "enum", true, "info, warning, elevated, critical."),
                    Field("riskState", "enum", true, "Healthy, Caution, Elevated, Defensive, or Critical."),
                    Field("detail", "string", true, "Plain-language reason without financial advice."),
                    Field("createdAt", "timestamp", true, "Audit-friendly event time.")
                }
            },
            {
                "ScannerOutput",
                SharedModelContractVersion(),
                "Signal",
                "Signal/scanner contract with cautious explanation and data confidence.",
                {
                    Field("symbol", "string", true, "Symbol contract key."),
                    Field("score", "integer", true, "Normalized research score."),
                    Field("confidence", "integer", true, "Confidence adjusted by data quality."),
                    Field("rating", "enum", true, "Watch, Review, Caution, Avoid, or similar non-advice posture."),
                    Field("thesis", "string", true, "Why the signal exists.")
                }
            },
            {
                "AISummary",
                SharedModelContractVersion(),
                "AI",
                "Cautious research summary with source grounding and explicit boundaries.",
                {
                    Field("scope", "enum", true, "symbol, portfolio, strategy, market, or session."),
                    Field("summary", "string", true, "Disciplined explanation."),
                    Field("evidence", "array", false, "References to journal, quote, scanner, report, or timeline inputs."),
                    Field("confidence", "number", true, "Confidence based on source quality.")
                }
            },
            {
                "ProviderHealth",
                SharedModelContractVersion(),
                "Providers",
                "Provider reliability and fallback routing contract.",
                {
                    Field("provider", "string", true, "Provider or cache layer."),
                    Field("status", "enum", true, "healthy, degraded, rate_limited, failed, fallback."),
                    Field("healthScore", "integer", true, "0-100 operational score."),
                    Field("detail", "string", false, "Last error or routing note.")
                }
            }
        };
    }

    std::vector<InfoItem> BuildSharedModelContractRows()
    {
        std::vector<InfoItem> rows;
        for (const SharedModelContract& contract : BuildSharedModelContracts())
        {
            int required = 0;
            for (const SharedModelField& field : contract.fields)
            {
                if (field.required)
                    ++required;
            }
            std::ostringstream value;
            value << required << "/" << contract.fields.size() << " required";
            rows.push_back(Row(contract.name, contract.owner, value.str(), contract.detail));
        }
        rows.push_back(Row("Contract version", "Stable draft", SharedModelContractVersion(), "Desktop, Core, Web, SDK, plugins, and reports should converge on these names, timestamps, enums, and freshness states."));
        return rows;
    }
}
