#pragma once

#include "imgui.h"

namespace aegis::theme
{
    inline constexpr ImVec4 Background = ImVec4(0.035f, 0.047f, 0.065f, 1.0f);
    inline constexpr ImVec4 Panel = ImVec4(0.060f, 0.078f, 0.102f, 1.0f);
    inline constexpr ImVec4 PanelRaised = ImVec4(0.078f, 0.100f, 0.128f, 1.0f);
    inline constexpr ImVec4 Text = ImVec4(0.900f, 0.930f, 0.970f, 1.0f);
    inline constexpr ImVec4 MutedText = ImVec4(0.580f, 0.650f, 0.720f, 1.0f);
    inline constexpr ImVec4 AnalyticsBlue = ImVec4(0.260f, 0.560f, 0.950f, 1.0f);
    inline constexpr ImVec4 SignalGreen = ImVec4(0.220f, 0.780f, 0.500f, 1.0f);
    inline constexpr ImVec4 RiskOrange = ImVec4(0.950f, 0.560f, 0.180f, 1.0f);
    inline constexpr ImVec4 RiskRed = ImVec4(0.930f, 0.260f, 0.300f, 1.0f);
    inline constexpr ImVec4 Fresh = ImVec4(0.220f, 0.780f, 0.500f, 1.0f);
    inline constexpr ImVec4 Stale = ImVec4(0.950f, 0.560f, 0.180f, 1.0f);
    inline constexpr ImVec4 Fallback = ImVec4(0.600f, 0.680f, 0.760f, 1.0f);
}
