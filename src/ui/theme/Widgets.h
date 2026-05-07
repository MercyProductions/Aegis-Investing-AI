#pragma once

#include "Colors.h"
#include "Spacing.h"

#include "imgui.h"

namespace aegis::theme
{
    inline void ApplyTerminalPanelStyle()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, PanelRadius);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, PanelRadius);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.145f, 0.180f, 0.225f, 1.0f));
    }

    inline void PopTerminalPanelStyle()
    {
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}
