#pragma once

#include "ui/AuralithTheme.h"

#include <cstddef>
#include <string>
#include <vector>

namespace auralith::ui
{
    enum class ButtonVariant
    {
        Primary,
        Secondary,
        Ghost,
        Danger,
        Compact
    };

    struct MetricCardSpec
    {
        std::string label;
        std::string value;
        std::string delta;
        std::string helper;
        Tone tone = Tone::Neutral;
        float progress = -1.0f;
        bool sparkline = false;
    };

    struct StatusPillSpec
    {
        std::string label;
        Tone tone = Tone::Neutral;
        std::string tooltip;
        const char* icon = nullptr;
    };

    struct EmptyStateSpec
    {
        const char* icon = nullptr;
        std::string title;
        std::string helper;
        std::string action_label;
    };

    bool Button(const char* label, ButtonVariant variant = ButtonVariant::Secondary, ImVec2 size = ImVec2(0, 0), bool enabled = true);
    bool PrimaryButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
    bool SecondaryButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
    bool GhostButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
    bool DangerButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
    bool IconButton(const char* id, const char* icon, const char* tooltip = nullptr, Tone tone = Tone::Neutral, ImVec2 size = ImVec2(34, 34));
    bool TableActionButton(const char* label);

    bool NavItem(const char* label, const char* icon, bool active, ImVec2 size = ImVec2(0, 42));
    void LogoCard(float width, const char* product = "Auralith", const char* subtitle = "Research Terminal");

    bool BeginCard(const char* id, const char* title = nullptr, const char* subtitle = nullptr, ImVec2 size = ImVec2(0, 0), bool elevated = false, ImGuiWindowFlags flags = 0);
    void EndCard();
    void SectionHeader(const char* title, const char* subtitle = nullptr);
    void MetricCard(const MetricCardSpec& spec, ImVec2 size = ImVec2(0, 102));
    void StatusPill(const StatusPillSpec& spec);
    bool Toggle(const char* label, bool* value, const char* helper = nullptr);
    void ProgressBar(const char* id, float value, ImVec2 size = ImVec2(0, 10), bool show_text = false, Tone tone = Tone::Success, const char* tooltip = nullptr);
    bool SearchInput(const char* id, const char* hint, char* buffer, std::size_t buffer_size, float width = 0.0f);
    bool CommandBox(const char* id, const char* hint, char* buffer, std::size_t buffer_size, float width = 0.0f);
    bool BeginDataTable(const char* id, int columns, ImVec2 size = ImVec2(0, 0), ImGuiTableFlags extra_flags = 0);
    bool BeginDataTable(const char* id, int columns, ImGuiTableFlags extra_flags, ImVec2 size = ImVec2(0, 0));
    void EndDataTable();
    void EmptyState(const EmptyStateSpec& spec);
    void ChartPanelBackground(const char* id, ImVec2 size, const char* label = nullptr);
    void TextMuted(const char* text);
    void TextSecondary(const char* text);
}
