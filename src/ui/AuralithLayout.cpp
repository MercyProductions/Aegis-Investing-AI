#include "ui/AuralithLayout.h"

namespace auralith::ui
{
    bool PageHeader(const PageHeaderSpec& spec)
    {
        const Spacing& s = GetSpacing();
        bool action_clicked = false;

        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(s.small, s.micro));
        ImGui::TextUnformatted(spec.title.c_str());
        if (!spec.subtitle.empty())
            TextSecondary(spec.subtitle.c_str());
        ImGui::PopStyleVar();

        if (!spec.badges.empty())
        {
            ImGui::Dummy(ImVec2(1.0f, s.micro));
            for (std::size_t i = 0; i < spec.badges.size(); ++i)
            {
                if (i > 0)
                    ImGui::SameLine();
                StatusPill(spec.badges[i]);
            }
        }
        ImGui::EndGroup();

        if (!spec.primary_action.empty())
        {
            const float width = 168.0f;
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - width + ImGui::GetCursorPosX());
            action_clicked = Button(spec.primary_action.c_str(),
                spec.action_tone == Tone::Danger ? ButtonVariant::Danger : ButtonVariant::Primary,
                ImVec2(width, 36.0f));
        }

        ImGui::Dummy(ImVec2(1.0f, s.section));
        return action_clicked;
    }

    CommandBarResult CommandBar(const CommandBarSpec& spec)
    {
        CommandBarResult result;
        const Spacing& s = GetSpacing();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(s.small, s.small));
        if (!spec.market_state.empty())
        {
            StatusPill({ spec.market_state, Tone::Info, "Current market session state.", "M" });
            ImGui::SameLine();
        }
        if (!spec.provider_state.empty())
        {
            StatusPill({ spec.provider_state, Tone::Info, "Provider freshness and route state.", "P" });
            ImGui::SameLine();
        }

        if (spec.command_buffer != nullptr)
        {
            if (spec.focus_command)
                ImGui::SetKeyboardFocusHere();
            result.command_submitted = CommandBox("##auralith_command", "Command or symbol", spec.command_buffer, spec.command_buffer_size, 250.0f);
            ImGui::SameLine();
        }

        if (spec.search_buffer != nullptr)
        {
            SearchInput("##auralith_symbol_search", "Search symbols", spec.search_buffer, spec.search_buffer_size, 176.0f);
            ImGui::SameLine();
        }

        result.refresh_clicked = SecondaryButton(spec.refreshing ? "Refreshing..." : "Refresh", ImVec2(104.0f, 34.0f), !spec.refreshing);
        ImGui::SameLine();

        const std::string alert_label = "Alerts " + std::to_string(spec.unread_alerts);
        result.alerts_clicked = Button(alert_label.c_str(), ButtonVariant::Ghost, ImVec2(96.0f, 34.0f));
        ImGui::SameLine();

        result.open_web_clicked = Button("Open Web", ButtonVariant::Ghost, ImVec2(102.0f, 34.0f));
        ImGui::SameLine();

        result.theme_clicked = IconButton("theme_toggle", "T", "Toggle light/dark theme", Tone::Info, ImVec2(34.0f, 34.0f));
        ImGui::SameLine();
        StatusPill({ spec.shortcut_hint, Tone::Muted, "Open the Auralith command palette.", "K" });

        ImGui::PopStyleVar();
        return result;
    }

    bool BeginPageSection(const char* id, const char* title, const char* subtitle, ImVec2 size, bool elevated)
    {
        return BeginCard(id, title, subtitle, size, elevated, ImGuiWindowFlags_NoScrollbar);
    }

    void EndPageSection()
    {
        EndCard();
    }

    void BeginTwoColumn(float left_width, float gap)
    {
        ImGui::BeginGroup();
        ImGui::PushID("auralith_two_column");
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, GetSpacing().small));
        ImGui::BeginChild("left", ImVec2(left_width, 0), false);
    }

    void NextColumn()
    {
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("right", ImVec2(0, 0), false);
    }

    void EndTwoColumn()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopID();
        ImGui::EndGroup();
    }
}
