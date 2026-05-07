#include "ui/SettingsTab.h"

#include "Diagnostics.h"
#include "Platform.h"
#include "ProviderLayer.h"
#include "SettingsService.h"
#include "ui/AuralithLayout.h"
#include "ui/AuralithWidgets.h"

#include "imgui.h"

namespace auralith::ui
{
    namespace
    {
        void Section(SettingsTabContext& ctx, const char* title, const char* subtitle = nullptr)
        {
            ctx.section_title(title, subtitle);
        }
    }

    void RenderSettingsTab(SettingsTabContext& ctx)
    {
        PageHeader({
            "Settings",
            "Market data, safety, web bridge, appearance, storage, diagnostics, and keyboard preferences.",
            {
            { ctx.config.paper_only_mode ? "Paper Mode: ON" : "Paper Mode: Review", ctx.config.paper_only_mode ? Tone::Info : Tone::Warning, "Paper-first safety setting.", "P" },
                { ctx.config.require_manual_confirmation ? "Manual Confirmation" : "Review Confirmation", ctx.config.require_manual_confirmation ? Tone::Info : Tone::Warning, "Execution workflows require manual review.", "M" }
            },
            "",
            Tone::Info
        });

        if (BeginCard("settings_data", "Market Data", "Provider keys, watchlist scope, refresh cadence, and cache policy.", ImVec2(ImGui::GetContentRegionAvail().x * 0.52f, 0), true))
        {
        Section(ctx, "Market Data");
        ImGui::InputText("Alpha Vantage key", ctx.api_key_buffer, ctx.api_key_buffer_size, ImGuiInputTextFlags_Password);
        ImGui::InputTextMultiline("Watchlist", ctx.watchlist_buffer, ctx.watchlist_buffer_size, ImVec2(0, 88.0f));
        ImGui::SliderInt("Refresh seconds", &ctx.config.refresh_seconds, 30, 900);
        ImGui::SliderInt("Max symbols", &ctx.config.max_symbols, 1, 50);
        ImGui::SliderInt("Model count", &ctx.config.model_count, 2, 32);
        ImGui::InputText("SEC user agent/contact", ctx.sec_user_agent_buffer, ctx.sec_user_agent_buffer_size);
        ImGui::SliderInt("Quote TTL sec", &ctx.config.alpha_quote_ttl_seconds, 15, 900);
        ImGui::SliderInt("History cache hr", &ctx.config.history_cache_hours, 1, 720);
        ImGui::SliderInt("Research cache hr", &ctx.config.research_cache_hours, 1, 720);
        ImGui::SliderInt("Max cache MB", &ctx.config.max_cache_mb, 25, 2048);
        Toggle("Force Live Refresh", &ctx.config.force_live_refresh, "Bypass provider cache during manual refresh workflows.");
        Toggle("Notifications", &ctx.config.notifications_enabled, "Allow local toast and Windows notification workflows.");
        if (ctx.small_action_button("Save Settings"))
            ctx.save_settings();
        ImGui::SameLine();
        if (ctx.small_action_button(ctx.validation_in_flight ? "Validating..." : "Validate Key"))
            ctx.begin_validation();
        ImGui::TextWrapped("%s", ctx.validation_status.c_str());
        ImGui::Separator();
        ctx.text_muted("Alpha Vantage GLOBAL_QUOTE is used for direct quotes; plan freshness depends on the API key entitlement.");
        ImGui::Spacing();
        ctx.render_info_table("Storage Plan", aegis::BuildStorageMigrationRows());
        ImGui::Spacing();
        ctx.render_info_table("Settings Health", aegis::BuildSettingsHealthRows(ctx.config));
        }
        EndCard();

        ImGui::SameLine();
        if (BeginCard("settings_auth", "Website Auth Bridge", "Local-first account bridge, appearance, safety, diagnostics, and session state.", ImVec2(0, 0), true))
        {
        Section(ctx, "Website Auth Bridge");
        ImGui::InputText("Web cockpit URL", ctx.website_base_buffer, ctx.website_base_buffer_size);
        ImGui::InputText("Auth base URL", ctx.auth_base_buffer, ctx.auth_base_buffer_size);
        ImGui::InputText("Username", ctx.login_user_buffer, ctx.login_user_buffer_size);
        ImGui::InputText("Password", ctx.login_password_buffer, ctx.login_password_buffer_size, ImGuiInputTextFlags_Password);
        Toggle("Remember Device", &ctx.remember_me, "Keep the web bridge credentials in Auralith's local credential store.");
        if (Toggle("Compact Dashboard", &ctx.compact_mode, "Tighten sidebar and panel density for smaller displays."))
            ctx.persist_ui_preferences("Compact dashboard preference saved.");
        if (Toggle("High Contrast Mode", &ctx.config.ui_high_contrast, "Increase borders and contrast for accessibility."))
            ctx.persist_ui_preferences("High contrast preference saved.");
        if (ImGui::SliderInt("Font scale", &ctx.config.font_scale_percent, 85, 150))
            ctx.persist_ui_preferences("Font scale preference saved.");
        if (ctx.small_action_button("Sign In"))
            ctx.sign_in();
        ImGui::SameLine();
        if (ctx.small_action_button("Clear Remembered"))
            ctx.clear_remembered();
        ImGui::SameLine();
        if (ctx.small_action_button("Clear All Secrets"))
            ctx.clear_all_secrets();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", ctx.authenticated ? "Website bridge connected for this session." : "Auralith research workflows remain local even when the website bridge is not signed in.");
        ImGui::Separator();
        Section(ctx, "Diagnostics");
        ImGui::TextWrapped("Structured logs: %s", (aegis::AppDataDirectory() / "diagnostics.jsonl").string().c_str());
        if (ctx.small_action_button("Open Diagnostics Folder"))
            ctx.open_diagnostics_folder();
        ImGui::SameLine();
        if (ctx.small_action_button("Run Self-Test"))
            ctx.run_self_test();
        ImGui::Spacing();
        ctx.render_info_table("Production Readiness", aegis::BuildProductionReadinessRows(ctx.config));
        ImGui::Spacing();
        ctx.render_info_table("Keyboard And Commands", ctx.build_shortcut_help_rows());
        ImGui::Spacing();
        ctx.render_info_table("Background Tasks", ctx.build_background_task_rows());
        ImGui::Spacing();
        const std::vector<std::string> recent = aegis::LoadRecentDiagnosticLines(4);
        if (!recent.empty())
        {
            Section(ctx, "Recent Diagnostics");
            for (const std::string& line : recent)
                ImGui::TextWrapped("%s", line.c_str());
        }
        }
        EndCard();
    }
}
