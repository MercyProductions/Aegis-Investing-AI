#include "CoreService.h"

#include "../ai/memory/IntelligenceMemory.h"
#include "../AppStorage.h"
#include "../audit/AuditManager.h"
#include "CoreIntelligence.h"
#include "../history/HistoricalStore.h"
#include "../intelligence/ResearchTimeline.h"
#include "../metrics/InternalMetrics.h"
#include "../persistence/CoreDatabase.h"
#include "../Platform.h"
#include "../professional/ProfessionalPlatform.h"
#include "../ProviderLayer.h"
#include "../reports/ReportComposer.h"
#include "../resilience/Resilience.h"
#include "../sdk/AuralithSDK.h"
#include "../services/BackgroundTaskManager.h"
#include "../shared/SharedModels.h"
#include "../workflows/DailyIntelligence.h"

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
            item.tag = "auralith-core";
            return item;
        }

        void AppendRows(std::vector<InfoItem>& target, const std::vector<InfoItem>& source)
        {
            target.insert(target.end(), source.begin(), source.end());
        }
    }

    CoreServiceStatus BuildCoreServiceHealth()
    {
        CoreServiceStatus status;
        const CoreDatabaseResult db = InitializeAuralithCoreDatabase();
        status.ok = db.ok;
        status.mode = "health";
        status.database = db.path.string();
        status.status = db.ok ? "AuralithCore health check passed." : "AuralithCore health check failed: " + db.error;
        status.rows = db.rows;
        status.rows.push_back(Row("Local API bridge", "Planned", "IPC/HTTP", "Desktop, web, and future companion clients should consume Core state through a local bridge."));
        status.rows.push_back(Row("Live execution", "Locked", "paper-first", "AuralithCore does not enable live trading paths."));
        AppendRows(status.rows, BuildCoreExtractionRoadmapRows());
        return status;
    }

    CoreServiceStatus RunCoreServiceCycle(bool refresh_providers)
    {
        CoreServiceStatus status;
        Config config = LoadConfig();
        SetHttpUserAgent(SecCompliantUserAgent(config));
        const CoreDatabaseResult db = InitializeAuralithCoreDatabase();
        const PersistentAppData app_data = LoadPersistentAppData();
        StockState state = refresh_providers ? BuildNativeStockState(config) : MakeDemoStockState();
        const CoreProviderSnapshot provider_snapshot = BuildCoreProviderSnapshot(config, state);

        status.ok = db.ok;
        status.mode = refresh_providers ? "sync" : "offline";
        status.database = db.path.string();
        status.status = db.ok ? "AuralithCore cycle completed." : "AuralithCore cycle completed with database warning: " + db.error;
        status.provider_source = provider_snapshot.source;
        status.provider_freshness = provider_snapshot.freshness;
        status.quote_count = provider_snapshot.quote_count;
        status.provider_confidence = provider_snapshot.confidence;
        status.rows.push_back(Row("Core cycle", status.mode, NowTimeLabel(), "Provider sync, alert monitoring, report readiness, AI memory, and workspace persistence are coordinated here."));
        AppendRows(status.rows, db.rows);
        AppendRows(status.rows, provider_snapshot.rows);
        AppendRows(status.rows, BuildCoreExtractionRoadmapRows());
        AppendRows(status.rows, BuildHistoricalStoreRows(config));
        AppendRows(status.rows, BuildMorningBriefingRows(config, state, app_data.holdings, app_data.price_alerts, app_data.alert_events, app_data.trade_plans));
        AppendRows(status.rows, BuildSymbolMemoryRows(state, app_data.symbol_notes, app_data.journal_entries, state.selected_symbol));
        AppendRows(status.rows, BuildPortfolioMemoryRows(state, app_data.holdings, app_data.journal_entries));
        AppendRows(status.rows, BuildResearchMemoryRows(state, app_data.trade_plans, app_data.journal_entries));
        AppendRows(status.rows, BuildSessionMemoryRows(config, state));
        AppendRows(status.rows, BuildResearchTimelineRows(state, state.selected_symbol, app_data.alert_events, app_data.trade_plans, app_data.journal_entries, app_data.symbol_notes));
        AppendRows(status.rows, BuildReportEngineRows(config, state));
        AppendRows(status.rows, BuildResilienceRows(config, state));
        AppendRows(status.rows, BuildRecoveryPlanRows());
        AppendRows(status.rows, BuildInternalMetricsRows(config, state));
        AppendRows(status.rows, BuildSharedModelContractRows());
        AppendRows(status.rows, BuildSdkModuleRows());
        AppendRows(status.rows, BuildProfessionalArchitectureRows());
        AppendRows(status.rows, BuildEnterpriseStabilityRows(config, state));
        AppendRows(status.rows, BuildDeploymentPipelineRows());
        AppendRows(status.rows, BuildPluginMarketplaceRows());
        AppendRows(status.rows, BuildAuditComplianceRows(config));
        std::string persistence_status;
        const bool persisted = PersistCoreServiceSnapshot(state, status.rows, db.path, &persistence_status);
        status.rows.push_back(Row("SQLite intelligence snapshot", persisted ? "Persisted" : "Review", db.path.string(), persistence_status, persisted ? "Ready" : "Review"));
        status.ok = status.ok && persisted;
        RecordManagedAuditEvent({ "AuralithCore", refresh_providers ? "core.sync_cycle" : "core.offline_cycle", "", status.status });
        return status;
    }

    std::string CoreServiceStatusJson(const CoreServiceStatus& status)
    {
        std::ostringstream json;
        json << "{";
        json << "\"ok\":" << (status.ok ? "true" : "false") << ",";
        json << "\"app\":\"AuralithCore\",";
        json << "\"mode\":\"" << EscapeJson(status.mode) << "\",";
        json << "\"database\":\"" << EscapeJson(status.database) << "\",";
        json << "\"status\":\"" << EscapeJson(status.status) << "\",";
        json << "\"providerSource\":\"" << EscapeJson(status.provider_source) << "\",";
        json << "\"providerFreshness\":\"" << EscapeJson(status.provider_freshness) << "\",";
        json << "\"quoteCount\":" << status.quote_count << ",";
        json << "\"providerConfidence\":" << status.provider_confidence << ",";
        json << "\"rows\":" << status.rows.size();
        json << "}";
        return json.str();
    }
}
