#pragma once

#include "../AdvancedAnalytics.h"
#include "../Platform.h"
#include "../StockData.h"

#include <vector>

namespace aegis
{
    std::vector<InfoItem> BuildProfessionalArchitectureRows();
    std::vector<InfoItem> BuildVisualStrategyBuilderRows();
    std::vector<InfoItem> BuildMultiTimeframeIntelligenceRows(const StockState& state);
    std::vector<InfoItem> BuildResearchCollectionRows(const StockState& state);
    std::vector<InfoItem> BuildCorrelationExposureRows(const StockState& state, const std::vector<PortfolioHolding>& holdings);
    std::vector<InfoItem> BuildScenarioSimulationRows(const StockState& state, const std::vector<PortfolioHolding>& holdings);
    std::vector<InfoItem> BuildResearchCopilotRows(const StockState& state, const std::vector<JournalEntry>& journal);
    std::vector<InfoItem> BuildLongTermPortfolioIntelligenceRows(const StockState& state, const std::vector<PortfolioHolding>& holdings, const std::vector<JournalEntry>& journal);
    std::vector<InfoItem> BuildEnterpriseStabilityRows(const Config& config, const StockState& state);
    std::vector<InfoItem> BuildDeploymentPipelineRows();
    std::vector<InfoItem> BuildDocumentationStandardsRows();
    std::vector<InfoItem> BuildPluginMarketplaceRows();
    std::vector<InfoItem> BuildCloudSyncPlanningRows();
    std::vector<InfoItem> BuildCrossDeviceEcosystemRows();
    std::vector<InfoItem> BuildResearchIdentityRows();
}
