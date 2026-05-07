#pragma once

#include "imgui.h"

namespace auralith::ui
{
    enum class Tone
    {
        Neutral,
        Success,
        Info,
        Warning,
        Danger,
        Purple,
        Muted
    };

    struct Palette
    {
        ImVec4 background;
        ImVec4 shell;
        ImVec4 panel;
        ImVec4 panel_elevated;
        ImVec4 border;
        ImVec4 border_active;
        ImVec4 text_primary;
        ImVec4 text_secondary;
        ImVec4 text_muted;
        ImVec4 accent_green;
        ImVec4 accent_blue;
        ImVec4 warning;
        ImVec4 danger;
        ImVec4 purple;
    };

    struct Spacing
    {
        float micro = 4.0f;
        float small = 8.0f;
        float standard = 12.0f;
        float section = 16.0f;
        float page = 24.0f;
    };

    const Palette& GetPalette();
    const Spacing& GetSpacing();
    ImVec4 Color(Tone tone, float alpha = 1.0f);
    ImVec4 ToneBackground(Tone tone, float alpha = 0.14f);
    ImVec4 ToneBorder(Tone tone, float alpha = 0.28f);
    ImU32 U32(const ImVec4& color);
    ImU32 ColorU32(Tone tone, float alpha = 1.0f);

    void ApplyAuralithTheme(bool high_contrast = false);
    void PushPanelStyle(bool elevated = false);
    void PopPanelStyle();
}
