#include "ui/AuralithTheme.h"

#include <algorithm>

namespace auralith::ui
{
    namespace
    {
        ImVec4 Hex(unsigned int rgb, float alpha = 1.0f)
        {
            const float r = static_cast<float>((rgb >> 16) & 0xff) / 255.0f;
            const float g = static_cast<float>((rgb >> 8) & 0xff) / 255.0f;
            const float b = static_cast<float>(rgb & 0xff) / 255.0f;
            return ImVec4(r, g, b, alpha);
        }

        Palette g_palette{
            Hex(0x05070B),
            Hex(0x070B11),
            Hex(0x0B1018),
            Hex(0x111925),
            Hex(0xFFFFFF, 0.06f),
            Hex(0x5EA7FF, 0.30f),
            Hex(0xEDF5FB),
            Hex(0xA7B5C4),
            Hex(0x687787),
            Hex(0x62D28F),
            Hex(0x61A8FF),
            Hex(0xE6C35C),
            Hex(0xFF6B6B),
            Hex(0xA27CFF)
        };

        Spacing g_spacing{};

        ImVec4 WithAlpha(ImVec4 color, float alpha)
        {
            color.w = alpha;
            return color;
        }
    }

    const Palette& GetPalette()
    {
        return g_palette;
    }

    const Spacing& GetSpacing()
    {
        return g_spacing;
    }

    ImVec4 Color(Tone tone, float alpha)
    {
        const Palette& p = GetPalette();
        switch (tone)
        {
        case Tone::Success: return WithAlpha(p.accent_green, alpha);
        case Tone::Info: return WithAlpha(p.accent_blue, alpha);
        case Tone::Warning: return WithAlpha(p.warning, alpha);
        case Tone::Danger: return WithAlpha(p.danger, alpha);
        case Tone::Purple: return WithAlpha(p.purple, alpha);
        case Tone::Muted: return WithAlpha(p.text_muted, alpha);
        case Tone::Neutral:
        default: return WithAlpha(p.text_secondary, alpha);
        }
    }

    ImVec4 ToneBackground(Tone tone, float alpha)
    {
        ImVec4 color = Color(tone, alpha);
        color.x = std::min(color.x * 0.55f, 1.0f);
        color.y = std::min(color.y * 0.55f, 1.0f);
        color.z = std::min(color.z * 0.55f, 1.0f);
        return color;
    }

    ImVec4 ToneBorder(Tone tone, float alpha)
    {
        return Color(tone, alpha);
    }

    ImU32 U32(const ImVec4& color)
    {
        return ImGui::ColorConvertFloat4ToU32(color);
    }

    ImU32 ColorU32(Tone tone, float alpha)
    {
        return U32(Color(tone, alpha));
    }

    void ApplyAuralithTheme(bool high_contrast)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsDark();

        style.WindowRounding = 0.0f;
        style.ChildRounding = 10.0f;
        style.FrameRounding = 8.0f;
        style.PopupRounding = 10.0f;
        style.ScrollbarRounding = 12.0f;
        style.ScrollbarSize = high_contrast ? 12.0f : 9.0f;
        style.GrabRounding = 8.0f;
        style.TabRounding = 8.0f;
        style.FrameBorderSize = high_contrast ? 1.5f : 1.0f;
        style.ChildBorderSize = high_contrast ? 1.5f : 1.0f;
        style.PopupBorderSize = high_contrast ? 1.5f : 1.0f;
        style.WindowBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(10.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style.FramePadding = ImVec2(12.0f, 8.0f);
        style.WindowPadding = ImVec2(12.0f, 12.0f);
        style.CellPadding = ImVec2(9.0f, 7.0f);
        style.IndentSpacing = 16.0f;

        ImVec4* colors = style.Colors;
        const Palette& p = GetPalette();
        colors[ImGuiCol_Text] = p.text_primary;
        colors[ImGuiCol_TextDisabled] = p.text_muted;
        colors[ImGuiCol_WindowBg] = p.background;
        colors[ImGuiCol_ChildBg] = p.panel;
        colors[ImGuiCol_PopupBg] = ImVec4(p.panel_elevated.x, p.panel_elevated.y, p.panel_elevated.z, 0.99f);
        colors[ImGuiCol_Border] = high_contrast ? ImVec4(0.38f, 0.66f, 1.0f, 0.32f) : p.border;
        colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_Separator] = p.border;
        colors[ImGuiCol_FrameBg] = ImVec4(0.050f, 0.070f, 0.102f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.070f, 0.112f, 0.170f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.085f, 0.150f, 0.235f, 1.0f);
        colors[ImGuiCol_CheckMark] = p.accent_blue;
        colors[ImGuiCol_SliderGrab] = p.accent_blue;
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.76f, 1.0f, 1.0f);
        colors[ImGuiCol_Header] = ImVec4(0.070f, 0.125f, 0.200f, 0.78f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.090f, 0.170f, 0.285f, 0.90f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.115f, 0.220f, 0.365f, 1.0f);
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.044f, 0.062f, 0.092f, 1.0f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.030f, 0.042f, 0.062f, 0.76f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.045f, 0.060f, 0.088f, 0.80f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(1.0f, 1.0f, 1.0f, high_contrast ? 0.18f : 0.08f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(1.0f, 1.0f, 1.0f, high_contrast ? 0.13f : 0.045f);
        colors[ImGuiCol_Button] = ImVec4(0.055f, 0.085f, 0.125f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.075f, 0.135f, 0.215f, 1.0f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.105f, 0.185f, 0.310f, 1.0f);
        colors[ImGuiCol_PlotHistogram] = p.accent_blue;
        colors[ImGuiCol_PlotLines] = p.accent_blue;
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.28f, 0.58f, 1.0f, 0.24f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.28f, 0.58f, 1.0f, 0.42f);
    }

    void PushPanelStyle(bool elevated)
    {
        const Palette& p = GetPalette();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, elevated ? p.panel_elevated : p.panel);
        ImGui::PushStyleColor(ImGuiCol_Border, elevated ? p.border_active : p.border);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 14.0f));
    }

    void PopPanelStyle()
    {
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
}
