#include "ui/AuralithWidgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace auralith::ui
{
    namespace
    {
        ImVec4 ButtonColor(ButtonVariant variant, bool hovered, bool active, bool enabled)
        {
            const Palette& p = GetPalette();
            if (!enabled)
                return ImVec4(0.06f, 0.08f, 0.07f, 0.64f);

            float lift = active ? 0.09f : hovered ? 0.055f : 0.0f;
            switch (variant)
            {
            case ButtonVariant::Primary:
                return ImVec4(0.075f + lift, 0.215f + lift * 0.75f, 0.380f + lift, 1.0f);
            case ButtonVariant::Danger:
                return ImVec4(0.28f + lift, 0.08f + lift * 0.4f, 0.08f + lift * 0.4f, 1.0f);
            case ButtonVariant::Ghost:
                return hovered || active ? ImVec4(0.070f, 0.105f, 0.158f, 0.82f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
            case ButtonVariant::Compact:
                return hovered || active ? ImVec4(0.066f, 0.110f, 0.170f, 0.94f) : ImVec4(0.040f, 0.058f, 0.085f, 0.94f);
            case ButtonVariant::Secondary:
            default:
                return hovered || active ? p.panel_elevated : ImVec4(0.050f, 0.071f, 0.104f, 0.96f);
            }
        }

        ImVec4 ButtonBorder(ButtonVariant variant, bool hovered, bool active, bool enabled)
        {
            if (!enabled)
                return ImVec4(1, 1, 1, 0.035f);
            if (variant == ButtonVariant::Primary)
                return ImVec4(0.37f, 0.66f, 1.0f, active ? 0.54f : hovered ? 0.42f : 0.30f);
            if (variant == ButtonVariant::Danger)
                return ImVec4(1.0f, 0.42f, 0.42f, active ? 0.50f : hovered ? 0.38f : 0.24f);
            return hovered || active ? GetPalette().border_active : GetPalette().border;
        }

        bool CustomButton(const char* label, const char* icon, ButtonVariant variant, ImVec2 requested_size, bool enabled)
        {
            const ImGuiStyle& style = ImGui::GetStyle();
            const ImVec2 text_size = ImGui::CalcTextSize(label, nullptr, true);
            const float height = requested_size.y > 0.0f ? requested_size.y : (variant == ButtonVariant::Compact ? 28.0f : 36.0f);
            const float icon_w = icon != nullptr && icon[0] != '\0' ? 18.0f : 0.0f;
            const float width = requested_size.x > 0.0f
                ? requested_size.x
                : requested_size.x < 0.0f
                    ? std::max(1.0f, ImGui::GetContentRegionAvail().x + requested_size.x)
                    : text_size.x + icon_w + style.FramePadding.x * 2.0f + (icon_w > 0.0f ? 6.0f : 0.0f);
            const ImVec2 size(width, height);
            const ImVec2 pos = ImGui::GetCursorScreenPos();

            ImGui::PushID(label);
            if (!enabled)
                ImGui::BeginDisabled();
            const bool clicked = ImGui::InvisibleButton("auralith_button", size);
            if (!enabled)
                ImGui::EndDisabled();

            const bool hovered = enabled && ImGui::IsItemHovered();
            const bool held = enabled && ImGui::IsItemActive();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(ButtonColor(variant, hovered, held, enabled)), 8.0f);
            draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(ButtonBorder(variant, hovered, held, enabled)), 8.0f);

            const ImVec4 text_color = enabled ? GetPalette().text_primary : GetPalette().text_muted;
            float text_x = pos.x + style.FramePadding.x;
            if (icon_w > 0.0f)
            {
                draw->AddText(ImVec2(text_x, pos.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f), ColorU32(variant == ButtonVariant::Danger ? Tone::Danger : Tone::Info), icon);
                text_x += icon_w + 6.0f;
            }
            draw->AddText(ImVec2(text_x, pos.y + (size.y - text_size.y) * 0.5f), U32(text_color), label);
            ImGui::PopID();
            return clicked && enabled;
        }
    }

    bool Button(const char* label, ButtonVariant variant, ImVec2 size, bool enabled)
    {
        return CustomButton(label, nullptr, variant, size, enabled);
    }

    bool PrimaryButton(const char* label, ImVec2 size, bool enabled)
    {
        return Button(label, ButtonVariant::Primary, size, enabled);
    }

    bool SecondaryButton(const char* label, ImVec2 size, bool enabled)
    {
        return Button(label, ButtonVariant::Secondary, size, enabled);
    }

    bool GhostButton(const char* label, ImVec2 size, bool enabled)
    {
        return Button(label, ButtonVariant::Ghost, size, enabled);
    }

    bool DangerButton(const char* label, ImVec2 size, bool enabled)
    {
        return Button(label, ButtonVariant::Danger, size, enabled);
    }

    bool IconButton(const char* id, const char* icon, const char* tooltip, Tone tone, ImVec2 size)
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::PushID(id);
        const bool clicked = ImGui::InvisibleButton("icon_button", size);
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(hovered ? ToneBackground(tone, 0.22f) : ToneBackground(Tone::Neutral, 0.08f)), 8.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(hovered ? ToneBorder(tone, 0.36f) : GetPalette().border), 8.0f);
        const ImVec2 icon_size = ImGui::CalcTextSize(icon);
        draw->AddText(ImVec2(pos.x + (size.x - icon_size.x) * 0.5f, pos.y + (size.y - icon_size.y) * 0.5f), ColorU32(tone), icon);
        if (hovered && tooltip != nullptr)
            ImGui::SetTooltip("%s", tooltip);
        ImGui::PopID();
        return clicked;
    }

    bool TableActionButton(const char* label)
    {
        return Button(label, ButtonVariant::Compact, ImVec2(0, 26.0f));
    }

    bool NavItem(const char* label, const char* icon, bool active, ImVec2 size)
    {
        if (size.x <= 0.0f)
            size.x = ImGui::GetContentRegionAvail().x;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::PushID(label);
        const bool clicked = ImGui::InvisibleButton("nav_item", size);
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        const ImU32 bg = U32(active ? ToneBackground(Tone::Info, 0.20f) : hovered ? ToneBackground(Tone::Info, 0.10f) : ImVec4(0, 0, 0, 0));
        const ImU32 border = U32(active ? GetPalette().border_active : hovered ? ToneBorder(Tone::Info, 0.18f) : ImVec4(1, 1, 1, 0.0f));
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, 9.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), border, 9.0f);
        if (active)
            draw->AddRectFilled(ImVec2(pos.x, pos.y + 8.0f), ImVec2(pos.x + 3.0f, pos.y + size.y - 8.0f), ColorU32(Tone::Info), 2.0f);

        const ImU32 icon_color = ColorU32(active ? Tone::Info : Tone::Muted, active ? 1.0f : hovered ? 0.9f : 0.72f);
        const ImU32 text_color = U32(active ? GetPalette().text_primary : hovered ? GetPalette().text_secondary : GetPalette().text_muted);
        draw->AddText(ImVec2(pos.x + 13.0f, pos.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f), icon_color, icon != nullptr ? icon : "");
        draw->AddText(ImVec2(pos.x + 40.0f, pos.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f), text_color, label);
        ImGui::PopID();
        return clicked;
    }

    void LogoCard(float width, const char* product, const char* subtitle)
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(width, 78.0f);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(GetPalette().panel_elevated), 10.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(GetPalette().border_active), 10.0f);
        draw->AddTriangleFilled(ImVec2(pos.x + 20.0f, pos.y + 54.0f), ImVec2(pos.x + 43.0f, pos.y + 18.0f), ImVec2(pos.x + 55.0f, pos.y + 47.0f), ColorU32(Tone::Info));
        draw->AddRectFilled(ImVec2(pos.x + 58.0f, pos.y + 33.0f), ImVec2(pos.x + 64.0f, pos.y + 54.0f), ColorU32(Tone::Info, 0.86f), 2.0f);
        draw->AddRectFilled(ImVec2(pos.x + 68.0f, pos.y + 25.0f), ImVec2(pos.x + 74.0f, pos.y + 54.0f), U32(GetPalette().text_secondary), 2.0f);
        draw->AddText(ImVec2(pos.x + 88.0f, pos.y + 18.0f), U32(GetPalette().text_primary), product);
        draw->AddText(ImVec2(pos.x + 89.0f, pos.y + 42.0f), U32(GetPalette().text_secondary), subtitle);
        ImGui::Dummy(size);
    }

    bool BeginCard(const char* id, const char* title, const char* subtitle, ImVec2 size, bool elevated, ImGuiWindowFlags flags)
    {
        PushPanelStyle(elevated);
        const bool open = ImGui::BeginChild(id, size, true, flags);
        if (title != nullptr && title[0] != '\0')
            SectionHeader(title, subtitle);
        return open;
    }

    void EndCard()
    {
        ImGui::EndChild();
        PopPanelStyle();
    }

    void SectionHeader(const char* title, const char* subtitle)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, GetPalette().text_primary);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        if (subtitle != nullptr && subtitle[0] != '\0')
        {
            ImGui::PushStyleColor(ImGuiCol_Text, GetPalette().text_secondary);
            ImGui::TextWrapped("%s", subtitle);
            ImGui::PopStyleColor();
        }
        ImGui::Dummy(ImVec2(1.0f, 4.0f));
    }

    void MetricCard(const MetricCardSpec& spec, ImVec2 size)
    {
        if (size.x <= 0.0f)
            size.x = ImGui::GetContentRegionAvail().x;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::PushID(spec.label.c_str());
        ImGui::InvisibleButton("metric_card", size);
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(hovered ? GetPalette().panel_elevated : GetPalette().panel), 10.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(hovered ? ToneBorder(spec.tone, 0.25f) : GetPalette().border), 10.0f);
        draw->AddText(ImVec2(pos.x + 14.0f, pos.y + 12.0f), U32(GetPalette().text_secondary), spec.label.c_str());
        draw->AddText(ImVec2(pos.x + 14.0f, pos.y + 36.0f), U32(GetPalette().text_primary), spec.value.c_str());
        if (!spec.delta.empty())
            draw->AddText(ImVec2(pos.x + 14.0f, pos.y + size.y - 34.0f), ColorU32(spec.tone), spec.delta.c_str());
        if (!spec.helper.empty())
            draw->AddText(ImVec2(pos.x + 14.0f, pos.y + size.y - 18.0f), U32(GetPalette().text_muted), spec.helper.c_str());
        if (spec.progress >= 0.0f)
        {
            const float t = std::clamp(spec.progress, 0.0f, 1.0f);
            const ImVec2 bar_a(pos.x + 14.0f, pos.y + size.y - 10.0f);
            const ImVec2 bar_b(pos.x + size.x - 14.0f, pos.y + size.y - 5.0f);
            draw->AddRectFilled(bar_a, bar_b, U32(ImVec4(1, 1, 1, 0.055f)), 99.0f);
            draw->AddRectFilled(bar_a, ImVec2(bar_a.x + (bar_b.x - bar_a.x) * t, bar_b.y), ColorU32(spec.tone), 99.0f);
        }
        ImGui::PopID();
    }

    void StatusPill(const StatusPillSpec& spec)
    {
        const ImVec2 text_size = ImGui::CalcTextSize(spec.label.c_str());
        const float icon_w = spec.icon != nullptr ? ImGui::CalcTextSize(spec.icon).x + 7.0f : 0.0f;
        const ImVec2 size(text_size.x + icon_w + 20.0f, 26.0f);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::PushID(spec.label.c_str());
        ImGui::InvisibleButton("status_pill", size);
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(ToneBackground(spec.tone, hovered ? 0.20f : 0.13f)), 99.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(ToneBorder(spec.tone, hovered ? 0.40f : 0.24f)), 99.0f);
        float x = pos.x + 10.0f;
        if (spec.icon != nullptr)
        {
            draw->AddText(ImVec2(x, pos.y + 5.0f), ColorU32(spec.tone), spec.icon);
            x += icon_w;
        }
        draw->AddText(ImVec2(x, pos.y + 5.0f), U32(GetPalette().text_primary), spec.label.c_str());
        if (hovered && !spec.tooltip.empty())
            ImGui::SetTooltip("%s", spec.tooltip.c_str());
        ImGui::PopID();
    }

    bool Toggle(const char* label, bool* value, const char* helper)
    {
        const float width = ImGui::GetContentRegionAvail().x;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(width, helper != nullptr ? 54.0f : 38.0f);
        ImGui::PushID(label);
        const bool clicked = ImGui::InvisibleButton("toggle", size);
        if (clicked)
            *value = !*value;
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(hovered ? GetPalette().panel_elevated : GetPalette().panel), 8.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(hovered ? GetPalette().border_active : GetPalette().border), 8.0f);
        draw->AddText(ImVec2(pos.x + 12.0f, pos.y + 9.0f), U32(GetPalette().text_primary), label);
        if (helper != nullptr)
            draw->AddText(ImVec2(pos.x + 12.0f, pos.y + 29.0f), U32(GetPalette().text_muted), helper);
        const ImVec2 track_a(pos.x + size.x - 58.0f, pos.y + 10.0f);
        const ImVec2 track_b(pos.x + size.x - 12.0f, pos.y + 32.0f);
        draw->AddRectFilled(track_a, track_b, U32(*value ? ToneBackground(Tone::Info, 0.48f) : ImVec4(1, 1, 1, 0.07f)), 99.0f);
        draw->AddRect(track_a, track_b, U32(*value ? ToneBorder(Tone::Info, 0.52f) : GetPalette().border), 99.0f);
        const float knob_x = *value ? track_b.x - 18.0f : track_a.x + 4.0f;
        draw->AddCircleFilled(ImVec2(knob_x + 7.0f, track_a.y + 11.0f), 7.0f, U32(*value ? GetPalette().accent_blue : GetPalette().text_muted));
        ImGui::PopID();
        return clicked;
    }

    void ProgressBar(const char* id, float value, ImVec2 size, bool show_text, Tone tone, const char* tooltip)
    {
        if (size.x <= 0.0f)
            size.x = ImGui::GetContentRegionAvail().x;
        const float t = std::clamp(value, 0.0f, 1.0f);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::PushID(id);
        ImGui::InvisibleButton("progress", size);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(ImVec4(1, 1, 1, 0.055f)), 99.0f);
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x * t, pos.y + size.y), ColorU32(tone), 99.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(GetPalette().border), 99.0f);
        if (show_text)
        {
            char text[16]{};
            std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(std::round(t * 100.0f)));
            draw->AddText(ImVec2(pos.x + size.x + 8.0f, pos.y - 3.0f), U32(GetPalette().text_secondary), text);
        }
        if (ImGui::IsItemHovered() && tooltip != nullptr)
            ImGui::SetTooltip("%s", tooltip);
        ImGui::PopID();
    }

    bool SearchInput(const char* id, const char* hint, char* buffer, std::size_t buffer_size, float width)
    {
        if (width > 0.0f)
            ImGui::SetNextItemWidth(width);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GetPalette().panel);
        ImGui::PushStyleColor(ImGuiCol_Border, GetPalette().border);
        const bool submitted = ImGui::InputTextWithHint(id, hint, buffer, buffer_size, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        return submitted;
    }

    bool CommandBox(const char* id, const char* hint, char* buffer, std::size_t buffer_size, float width)
    {
        return SearchInput(id, hint, buffer, buffer_size, width);
    }

    bool BeginDataTable(const char* id, int columns, ImVec2 size, ImGuiTableFlags extra_flags)
    {
        if (columns <= 0)
            return false;

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (size.x < 0.0f)
            size.x = 0.0f;
        if (size.y < 0.0f)
            size.y = 0.0f;

        const float estimated_height = size.y > 0.0f
            ? size.y
            : std::max(54.0f, ImGui::GetTextLineHeightWithSpacing() * 3.0f);
        const ImVec2 visibility_probe(
            std::max(1.0f, size.x > 0.0f ? size.x : avail.x),
            std::max(1.0f, estimated_height));

        // ImGui tables assert if their host clip rect has no vertical area.
        // This can happen when a long scrollable Auralith panel keeps laying out
        // lower tables after the visible child region has been exhausted.
        if (avail.x <= 1.0f || !ImGui::IsRectVisible(visibility_probe))
        {
            ImGui::Dummy(ImVec2(1.0f, estimated_height));
            return false;
        }

        extra_flags &= ~ImGuiTableFlags_Borders;
        ImGuiTableFlags flags =
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_Reorderable |
            ImGuiTableFlags_BordersOuter |
            ImGuiTableFlags_NoBordersInBody |
            extra_flags;
        if (size.y > 0.0f)
            flags |= ImGuiTableFlags_ScrollY;
        return ImGui::BeginTable(id, columns, flags, size);
    }

    bool BeginDataTable(const char* id, int columns, ImGuiTableFlags extra_flags, ImVec2 size)
    {
        return BeginDataTable(id, columns, size, extra_flags);
    }

    void EndDataTable()
    {
        ImGui::EndTable();
    }

    void EmptyState(const EmptyStateSpec& spec)
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 size(std::max(260.0f, avail.x), 132.0f);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(("empty_state_" + spec.title).c_str(), size);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(GetPalette().panel), 10.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(GetPalette().border), 10.0f);
        const char* icon = spec.icon != nullptr ? spec.icon : "i";
        draw->AddText(ImVec2(pos.x + 18.0f, pos.y + 18.0f), ColorU32(Tone::Info), icon);
        draw->AddText(ImVec2(pos.x + 44.0f, pos.y + 18.0f), U32(GetPalette().text_primary), spec.title.c_str());
        draw->AddText(ImVec2(pos.x + 44.0f, pos.y + 44.0f), U32(GetPalette().text_secondary), spec.helper.c_str());
        if (!spec.action_label.empty())
        {
            ImGui::SetCursorScreenPos(ImVec2(pos.x + 44.0f, pos.y + 82.0f));
            SecondaryButton(spec.action_label.c_str(), ImVec2(180.0f, 32.0f));
        }
    }

    void ChartPanelBackground(const char* id, ImVec2 size, const char* label)
    {
        if (size.x <= 0.0f)
            size.x = ImGui::GetContentRegionAvail().x;
        if (size.y <= 0.0f)
            size.y = 260.0f;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::PushID(id);
        ImGui::InvisibleButton("chart_panel", size);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(GetPalette().panel), 10.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), U32(GetPalette().border), 10.0f);
        for (int i = 1; i < 5; ++i)
        {
            const float y = pos.y + size.y * (static_cast<float>(i) / 5.0f);
            draw->AddLine(ImVec2(pos.x + 12.0f, y), ImVec2(pos.x + size.x - 12.0f, y), U32(ImVec4(1, 1, 1, 0.035f)));
        }
        for (int i = 1; i < 8; ++i)
        {
            const float x = pos.x + size.x * (static_cast<float>(i) / 8.0f);
            draw->AddLine(ImVec2(x, pos.y + 12.0f), ImVec2(x, pos.y + size.y - 12.0f), U32(ImVec4(1, 1, 1, 0.025f)));
        }
        if (label != nullptr)
            draw->AddText(ImVec2(pos.x + 14.0f, pos.y + 12.0f), U32(GetPalette().text_muted), label);
        ImGui::PopID();
    }

    void TextMuted(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, GetPalette().text_muted);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    void TextSecondary(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, GetPalette().text_secondary);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }
}
