#include "Json.h"
#include "Platform.h"
#include "StockData.h"
#include "AdvancedAnalytics.h"
#include "AlertEngine.h"
#include "AppSession.h"
#include "AppStorage.h"
#include "BrokerImport.h"
#include "Csv.h"
#include "Diagnostics.h"
#include "ImportValidation.h"
#include "PositionSizing.h"
#include "ProviderLayer.h"
#include "ReportExport.h"
#include "SelfTests.h"
#include "SettingsService.h"
#include "SymbolRules.h"
#include "WorkflowValidation.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <tchar.h>
#include <windows.h>

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    ID3D11Device* g_pd3dDevice = nullptr;
    ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
    IDXGISwapChain* g_pSwapChain = nullptr;
    bool g_SwapChainOccluded = false;
    UINT g_ResizeWidth = 0;
    UINT g_ResizeHeight = 0;
    ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    void ApplyTheme();
    void ApplyLightTheme();
    void ApplyHighContrastTheme(bool light_theme);
    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    ImFont* g_font_regular = nullptr;
    ImFont* g_font_bold = nullptr;
    ImFont* g_font_title = nullptr;
    constexpr int kMinWindowWidth = 1120;
    constexpr int kMinWindowHeight = 720;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

    ImU32 Col(float r, float g, float b, float a = 1.0f)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
    }

    ImVec4 V4(float r, float g, float b, float a = 1.0f)
    {
        return ImVec4(r, g, b, a);
    }

    int CurrentYtdTradingDays()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
        std::tm local_time{};
        localtime_s(&local_time, &timestamp);
        return std::clamp(((local_time.tm_yday + 1) * 5) / 7, 30, 252);
    }

    void TextMuted(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, V4(0.62f, 0.68f, 0.66f, 1.0f));
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    void TextValueColored(double value, const std::string& text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, value >= 0.0 ? V4(0.28f, 0.86f, 0.48f, 1.0f) : V4(1.0f, 0.42f, 0.42f, 1.0f));
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopStyleColor();
    }

    enum class IconKind
    {
        Dashboard,
        Watchlist,
        Scanner,
        Portfolio,
        Research,
        Chart,
        Compare,
        Journal,
        Integration,
        Settings,
        Refresh,
        Search,
        Shield
    };

    void DrawIcon(ImDrawList* draw, ImVec2 center, float size, IconKind icon, ImU32 color, float thickness = 1.8f)
    {
        const float r = size * 0.5f;
        switch (icon)
        {
        case IconKind::Dashboard:
            draw->AddRect(ImVec2(center.x - r * 0.7f, center.y - r * 0.7f), ImVec2(center.x - r * 0.1f, center.y - r * 0.1f), color, 2.5f, 0, thickness);
            draw->AddRect(ImVec2(center.x + r * 0.1f, center.y - r * 0.7f), ImVec2(center.x + r * 0.7f, center.y - r * 0.1f), color, 2.5f, 0, thickness);
            draw->AddRect(ImVec2(center.x - r * 0.7f, center.y + r * 0.1f), ImVec2(center.x - r * 0.1f, center.y + r * 0.7f), color, 2.5f, 0, thickness);
            draw->AddRect(ImVec2(center.x + r * 0.1f, center.y + r * 0.1f), ImVec2(center.x + r * 0.7f, center.y + r * 0.7f), color, 2.5f, 0, thickness);
            break;
        case IconKind::Watchlist:
            draw->AddLine(ImVec2(center.x - r * 0.75f, center.y + r * 0.55f), ImVec2(center.x - r * 0.22f, center.y + r * 0.02f), color, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.22f, center.y + r * 0.02f), ImVec2(center.x + r * 0.08f, center.y + r * 0.22f), color, thickness);
            draw->AddLine(ImVec2(center.x + r * 0.08f, center.y + r * 0.22f), ImVec2(center.x + r * 0.72f, center.y - r * 0.58f), color, thickness);
            break;
        case IconKind::Scanner:
            draw->AddCircle(center, r * 0.48f, color, 28, thickness);
            draw->AddLine(ImVec2(center.x + r * 0.34f, center.y + r * 0.34f), ImVec2(center.x + r * 0.76f, center.y + r * 0.76f), color, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.28f, center.y), ImVec2(center.x + r * 0.28f, center.y), color, thickness);
            break;
        case IconKind::Portfolio:
            draw->AddRect(ImVec2(center.x - r * 0.70f, center.y - r * 0.38f), ImVec2(center.x + r * 0.70f, center.y + r * 0.58f), color, 4.0f, 0, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.38f, center.y - r * 0.38f), ImVec2(center.x - r * 0.22f, center.y - r * 0.68f), color, thickness);
            draw->AddLine(ImVec2(center.x + r * 0.38f, center.y - r * 0.38f), ImVec2(center.x + r * 0.22f, center.y - r * 0.68f), color, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.18f, center.y + r * 0.10f), ImVec2(center.x + r * 0.18f, center.y + r * 0.10f), color, thickness);
            break;
        case IconKind::Research:
            draw->AddRect(ImVec2(center.x - r * 0.48f, center.y - r * 0.70f), ImVec2(center.x + r * 0.48f, center.y + r * 0.70f), color, 3.0f, 0, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.24f, center.y - r * 0.26f), ImVec2(center.x + r * 0.24f, center.y - r * 0.26f), color, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.24f, center.y + r * 0.02f), ImVec2(center.x + r * 0.18f, center.y + r * 0.02f), color, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.24f, center.y + r * 0.30f), ImVec2(center.x + r * 0.28f, center.y + r * 0.30f), color, thickness);
            break;
        case IconKind::Chart:
            draw->AddLine(ImVec2(center.x - r * 0.70f, center.y + r * 0.58f), ImVec2(center.x + r * 0.70f, center.y + r * 0.58f), color, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.70f, center.y + r * 0.58f), ImVec2(center.x - r * 0.70f, center.y - r * 0.62f), color, thickness);
            draw->AddRect(ImVec2(center.x - r * 0.48f, center.y - r * 0.18f), ImVec2(center.x - r * 0.24f, center.y + r * 0.42f), color, 2.0f, 0, thickness);
            draw->AddRect(ImVec2(center.x - r * 0.06f, center.y - r * 0.46f), ImVec2(center.x + r * 0.18f, center.y + r * 0.42f), color, 2.0f, 0, thickness);
            draw->AddRect(ImVec2(center.x + r * 0.36f, center.y - r * 0.05f), ImVec2(center.x + r * 0.60f, center.y + r * 0.42f), color, 2.0f, 0, thickness);
            break;
        case IconKind::Compare:
            draw->AddRect(ImVec2(center.x - r * 0.72f, center.y - r * 0.55f), ImVec2(center.x - r * 0.08f, center.y + r * 0.55f), color, 3.0f, 0, thickness);
            draw->AddRect(ImVec2(center.x + r * 0.08f, center.y - r * 0.55f), ImVec2(center.x + r * 0.72f, center.y + r * 0.55f), color, 3.0f, 0, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.55f, center.y + r * 0.22f), ImVec2(center.x - r * 0.25f, center.y - r * 0.10f), color, thickness);
            draw->AddLine(ImVec2(center.x + r * 0.25f, center.y + r * 0.20f), ImVec2(center.x + r * 0.55f, center.y - r * 0.24f), color, thickness);
            break;
        case IconKind::Journal:
            draw->AddRect(ImVec2(center.x - r * 0.56f, center.y - r * 0.68f), ImVec2(center.x + r * 0.56f, center.y + r * 0.68f), color, 3.0f, 0, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.28f, center.y - r * 0.32f), ImVec2(center.x + r * 0.30f, center.y - r * 0.32f), color, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.28f, center.y), ImVec2(center.x + r * 0.30f, center.y), color, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.28f, center.y + r * 0.32f), ImVec2(center.x + r * 0.16f, center.y + r * 0.32f), color, thickness);
            break;
        case IconKind::Integration:
            draw->AddCircle(ImVec2(center.x - r * 0.42f, center.y), r * 0.22f, color, 20, thickness);
            draw->AddCircle(ImVec2(center.x + r * 0.42f, center.y - r * 0.34f), r * 0.22f, color, 20, thickness);
            draw->AddCircle(ImVec2(center.x + r * 0.42f, center.y + r * 0.34f), r * 0.22f, color, 20, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.20f, center.y), ImVec2(center.x + r * 0.20f, center.y - r * 0.26f), color, thickness);
            draw->AddLine(ImVec2(center.x - r * 0.20f, center.y), ImVec2(center.x + r * 0.20f, center.y + r * 0.26f), color, thickness);
            break;
        case IconKind::Settings:
            for (int i = 0; i < 8; ++i)
            {
                const float a = static_cast<float>(i) * 3.14159265f * 0.25f;
                draw->AddLine(ImVec2(center.x + std::cos(a) * r * 0.50f, center.y + std::sin(a) * r * 0.50f), ImVec2(center.x + std::cos(a) * r * 0.78f, center.y + std::sin(a) * r * 0.78f), color, thickness);
            }
            draw->AddCircle(center, r * 0.40f, color, 28, thickness);
            draw->AddCircle(center, r * 0.15f, color, 18, thickness);
            break;
        case IconKind::Refresh:
            draw->PathArcTo(center, r * 0.60f, 0.20f, 4.75f, 28);
            draw->PathStroke(color, 0, thickness);
            draw->AddLine(ImVec2(center.x + r * 0.08f, center.y - r * 0.66f), ImVec2(center.x + r * 0.54f, center.y - r * 0.68f), color, thickness);
            draw->AddLine(ImVec2(center.x + r * 0.54f, center.y - r * 0.68f), ImVec2(center.x + r * 0.40f, center.y - r * 0.26f), color, thickness);
            break;
        case IconKind::Search:
            draw->AddCircle(ImVec2(center.x - r * 0.10f, center.y - r * 0.10f), r * 0.42f, color, 24, thickness);
            draw->AddLine(ImVec2(center.x + r * 0.22f, center.y + r * 0.22f), ImVec2(center.x + r * 0.66f, center.y + r * 0.66f), color, thickness);
            break;
        case IconKind::Shield:
            {
                ImVec2 p[6] = {
                    ImVec2(center.x, center.y - r * 0.82f),
                    ImVec2(center.x + r * 0.66f, center.y - r * 0.45f),
                    ImVec2(center.x + r * 0.56f, center.y + r * 0.45f),
                    ImVec2(center.x, center.y + r * 0.82f),
                    ImVec2(center.x - r * 0.56f, center.y + r * 0.45f),
                    ImVec2(center.x - r * 0.66f, center.y - r * 0.45f)
                };
                draw->AddPolyline(p, 6, color, ImDrawFlags_Closed, thickness);
                draw->AddLine(ImVec2(center.x - r * 0.24f, center.y + r * 0.36f), ImVec2(center.x, center.y - r * 0.34f), color, thickness);
                draw->AddLine(ImVec2(center.x + r * 0.24f, center.y + r * 0.36f), ImVec2(center.x, center.y - r * 0.34f), color, thickness);
            }
            break;
        }
    }

    bool ToolbarButton(const char* label, IconKind icon, const ImVec2& size, bool active = false)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::PushID(label);
        const bool clicked = ImGui::InvisibleButton("toolbar_button", size);
        const bool hovered = ImGui::IsItemHovered();
        const ImU32 bg = active ? Col(0.07f, 0.24f, 0.18f, 1.0f) : hovered ? Col(0.07f, 0.12f, 0.12f, 0.96f) : Col(0.035f, 0.052f, 0.052f, 0.94f);
        const ImU32 border = active ? Col(0.24f, 0.86f, 0.54f, 0.58f) : Col(0.75f, 0.90f, 0.86f, 0.13f);
        const ImU32 fg = active ? Col(0.46f, 1.0f, 0.68f, 1.0f) : Col(0.78f, 0.86f, 0.84f, 1.0f);
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, 8.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), border, 8.0f);
        DrawIcon(draw, ImVec2(pos.x + 22.0f, pos.y + size.y * 0.5f), 18.0f, icon, fg, 1.6f);
        draw->AddText(g_font_bold, 14.0f, ImVec2(pos.x + 42.0f, pos.y + (size.y - 17.0f) * 0.5f), fg, label);
        ImGui::PopID();
        return clicked;
    }

    bool SmallActionButton(const char* label)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, V4(0.07f, 0.18f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, V4(0.10f, 0.30f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, V4(0.12f, 0.38f, 0.24f, 1.0f));
        const bool clicked = ImGui::Button(label);
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        return clicked;
    }

    void MetricCard(const aegis::InfoItem& item, const ImVec2& size)
    {
        ImGui::BeginChild(item.name.c_str(), size, true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PushFont(g_font_bold);
        TextMuted(item.name.c_str());
        ImGui::PopFont();
        ImGui::Spacing();
        ImGui::PushFont(g_font_title);
        ImGui::TextUnformatted(item.value.c_str());
        ImGui::PopFont();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", item.detail.c_str());
        ImGui::EndChild();
    }

    void SectionTitle(const char* title, const char* subtitle = nullptr)
    {
        ImGui::PushFont(g_font_bold);
        ImGui::TextUnformatted(title);
        ImGui::PopFont();
        if (subtitle != nullptr && subtitle[0] != '\0')
            TextMuted(subtitle);
    }

    void DrawScorePill(int score, const ImVec2& size)
    {
        const float t = std::clamp(static_cast<float>(score) / 100.0f, 0.0f, 1.0f);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 bg = Col(0.035f, 0.052f, 0.052f, 1.0f);
        const ImU32 fill = score >= 64 ? Col(0.23f, 0.86f, 0.48f, 1.0f) : score >= 50 ? Col(0.88f, 0.72f, 0.26f, 1.0f) : Col(0.95f, 0.36f, 0.36f, 1.0f);
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, 5.0f);
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x * t, pos.y + size.y), fill, 5.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), Col(0.78f, 0.90f, 0.84f, 0.16f), 5.0f);
        ImGui::Dummy(size);
    }

    void PlotHistory(const std::vector<float>& values, const ImVec2& size)
    {
        if (values.empty())
        {
            ImGui::Dummy(size);
            return;
        }
        float min_value = FLT_MAX;
        float max_value = -FLT_MAX;
        for (float value : values)
        {
            min_value = std::min(min_value, value);
            max_value = std::max(max_value, value);
        }
        if (std::fabs(max_value - min_value) < 0.001f)
        {
            max_value += 1.0f;
            min_value -= 1.0f;
        }
        ImGui::PushStyleColor(ImGuiCol_FrameBg, V4(0.025f, 0.040f, 0.040f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotLines, values.back() >= values.front() ? V4(0.28f, 0.86f, 0.48f, 1.0f) : V4(1.0f, 0.42f, 0.42f, 1.0f));
        ImGui::PlotLines("##history", values.data(), static_cast<int>(values.size()), 0, nullptr, min_value, max_value, size);
        ImGui::PopStyleColor(2);
    }

    void DrawLabeledValue(const char* label, const std::string& value, const std::string& detail = {})
    {
        TextMuted(label);
        ImGui::PushFont(g_font_bold);
        ImGui::TextWrapped("%s", value.c_str());
        ImGui::PopFont();
        if (!detail.empty())
            ImGui::TextWrapped("%s", detail.c_str());
    }

    double MovingAverageAt(const std::vector<aegis::Candle>& candles, int index, int period)
    {
        if (candles.empty() || index < 0 || period <= 0)
            return 0.0;
        const int end = std::min(index, static_cast<int>(candles.size()) - 1);
        const int start = std::max(0, end - period + 1);
        double sum = 0.0;
        int count = 0;
        for (int i = start; i <= end; ++i)
        {
            sum += candles[static_cast<size_t>(i)].close;
            ++count;
        }
        return count > 0 ? sum / static_cast<double>(count) : 0.0;
    }

    double RsiAt(const std::vector<aegis::Candle>& candles, int index, int period)
    {
        if (candles.size() < 2 || index <= 0 || period <= 0)
            return 50.0;
        const int end = std::min(index, static_cast<int>(candles.size()) - 1);
        const int start = std::max(1, end - period + 1);
        double gains = 0.0;
        double losses = 0.0;
        int count = 0;
        for (int i = start; i <= end; ++i)
        {
            const double change = candles[static_cast<size_t>(i)].close - candles[static_cast<size_t>(i - 1)].close;
            if (change >= 0.0)
                gains += change;
            else
                losses += std::fabs(change);
            ++count;
        }
        if (count == 0)
            return 50.0;
        const double average_gain = gains / static_cast<double>(count);
        const double average_loss = losses / static_cast<double>(count);
        if (average_loss <= 0.000001)
            return average_gain > 0.0 ? 99.99 : 50.0;
        const double rs = average_gain / average_loss;
        return 100.0 - (100.0 / (1.0 + rs));
    }

    long long AverageVolumeAt(const std::vector<aegis::Candle>& candles, int index, int period)
    {
        if (candles.empty() || index < 0 || period <= 0)
            return 0;
        const int end = std::min(index, static_cast<int>(candles.size()) - 1);
        const int start = std::max(0, end - period + 1);
        long long sum = 0;
        int count = 0;
        for (int i = start; i <= end; ++i)
        {
            sum += candles[static_cast<size_t>(i)].volume;
            ++count;
        }
        return count > 0 ? sum / count : 0;
    }

    void PlotCandleChart(const std::vector<aegis::Candle>& candles, int visible_days, bool show_volume, const ImVec2& size)
    {
        if (candles.empty())
        {
            ImGui::Dummy(size);
            return;
        }

        const int count = std::max(1, std::min(visible_days, static_cast<int>(candles.size())));
        const int start = static_cast<int>(candles.size()) - count;
        double min_price = DBL_MAX;
        double max_price = -DBL_MAX;
        long long max_volume = 1;
        for (int i = start; i < static_cast<int>(candles.size()); ++i)
        {
            min_price = std::min(min_price, candles[static_cast<size_t>(i)].low);
            max_price = std::max(max_price, candles[static_cast<size_t>(i)].high);
            max_volume = std::max(max_volume, candles[static_cast<size_t>(i)].volume);
        }
        if (std::fabs(max_price - min_price) < 0.001)
        {
            max_price += 1.0;
            min_price -= 1.0;
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 end(pos.x + size.x, pos.y + size.y);
        ImGui::InvisibleButton("candle_chart", size);
        const bool hovered = ImGui::IsItemHovered();
        draw->AddRectFilled(pos, end, Col(0.018f, 0.030f, 0.030f, 1.0f), 8.0f);
        draw->AddRect(pos, end, Col(0.75f, 0.90f, 0.86f, 0.14f), 8.0f);

        const float pad = 14.0f;
        const float volume_h = show_volume ? size.y * 0.20f : 0.0f;
        const float plot_h = std::max(60.0f, size.y - pad * 2.0f - volume_h);
        const float plot_w = std::max(60.0f, size.x - pad * 2.0f);
        const float base_y = pos.y + pad + plot_h;
        const float price_range = static_cast<float>(max_price - min_price);
        const auto y_for_price = [&](double price) {
            return pos.y + pad + static_cast<float>((max_price - price) / price_range) * plot_h;
        };

        for (int grid = 1; grid < 4; ++grid)
        {
            const float y = pos.y + pad + plot_h * static_cast<float>(grid) / 4.0f;
            draw->AddLine(ImVec2(pos.x + pad, y), ImVec2(pos.x + pad + plot_w, y), Col(0.75f, 0.90f, 0.86f, 0.075f), 1.0f);
        }

        const float step = plot_w / static_cast<float>(std::max(1, count));
        const float candle_w = std::clamp(step * 0.58f, 2.0f, 9.0f);
        std::vector<ImVec2> close_points;
        close_points.reserve(static_cast<size_t>(count));

        for (int n = 0; n < count; ++n)
        {
            const aegis::Candle& candle = candles[static_cast<size_t>(start + n)];
            const float x = pos.x + pad + step * (static_cast<float>(n) + 0.5f);
            const float open_y = y_for_price(candle.open);
            const float close_y = y_for_price(candle.close);
            const float high_y = y_for_price(candle.high);
            const float low_y = y_for_price(candle.low);
            const bool up = candle.close >= candle.open;
            const ImU32 color = up ? Col(0.26f, 0.86f, 0.48f, 1.0f) : Col(1.0f, 0.38f, 0.38f, 1.0f);
            draw->AddLine(ImVec2(x, high_y), ImVec2(x, low_y), color, 1.1f);
            draw->AddRectFilled(ImVec2(x - candle_w * 0.5f, std::min(open_y, close_y)), ImVec2(x + candle_w * 0.5f, std::max(open_y, close_y) + 1.0f), color, 1.5f);
            close_points.push_back(ImVec2(x, close_y));

            if (show_volume)
            {
                const float vol_top = base_y + 10.0f + volume_h * (1.0f - static_cast<float>(candle.volume) / static_cast<float>(max_volume));
                const float vol_bottom = pos.y + size.y - pad;
                draw->AddRectFilled(ImVec2(x - candle_w * 0.5f, vol_top), ImVec2(x + candle_w * 0.5f, vol_bottom), Col(0.50f, 0.72f, 0.78f, up ? 0.34f : 0.20f), 1.0f);
            }
        }

        if (close_points.size() > 1)
            draw->AddPolyline(close_points.data(), static_cast<int>(close_points.size()), Col(0.55f, 0.88f, 1.0f, 0.72f), 0, 1.4f);

        if (hovered)
        {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const int index = std::clamp(static_cast<int>((mouse.x - (pos.x + pad)) / std::max(1.0f, step)), 0, count - 1);
            const int candle_index = start + index;
            const aegis::Candle& candle = candles[static_cast<size_t>(candle_index)];
            const float x = pos.x + pad + step * (static_cast<float>(index) + 0.5f);
            draw->AddLine(ImVec2(x, pos.y + pad), ImVec2(x, pos.y + size.y - pad), Col(0.95f, 1.0f, 0.96f, 0.28f), 1.0f);
            draw->AddLine(ImVec2(pos.x + pad, mouse.y), ImVec2(pos.x + pad + plot_w, mouse.y), Col(0.95f, 1.0f, 0.96f, 0.18f), 1.0f);
            const double sma20 = MovingAverageAt(candles, candle_index, 20);
            const double sma50 = MovingAverageAt(candles, candle_index, 50);
            const double rsi14 = RsiAt(candles, candle_index, 14);
            const long long average_volume = AverageVolumeAt(candles, candle_index, 20);
            const double bar_change = candle_index > 0 && candles[static_cast<size_t>(candle_index - 1)].close > 0.0
                ? ((candle.close - candles[static_cast<size_t>(candle_index - 1)].close) / candles[static_cast<size_t>(candle_index - 1)].close) * 100.0
                : 0.0;
            const double volume_ratio = average_volume > 0
                ? static_cast<double>(candle.volume) / static_cast<double>(average_volume)
                : 0.0;
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(candle.date.c_str());
            ImGui::Text("O %s  H %s  L %s  C %s",
                aegis::FormatCurrency(candle.open).c_str(),
                aegis::FormatCurrency(candle.high).c_str(),
                aegis::FormatCurrency(candle.low).c_str(),
                aegis::FormatCurrency(candle.close).c_str());
            ImGui::Text("Volume %s", aegis::FormatVolume(candle.volume).c_str());
            ImGui::Separator();
            ImGui::Text("SMA20 %s  SMA50 %s",
                aegis::FormatCurrency(sma20).c_str(),
                aegis::FormatCurrency(sma50).c_str());
            ImGui::Text("RSI14 %.1f  Bar %s", rsi14, aegis::FormatPercent(bar_change).c_str());
            ImGui::Text("Volume vs 20-bar avg %.2fx", volume_ratio);
            ImGui::EndTooltip();
        }
    }

    void SetBuffer(char* buffer, size_t size, const std::string& value)
    {
        if (size == 0)
            return;
        strncpy_s(buffer, size, value.c_str(), _TRUNCATE);
    }

    struct RefreshResult
    {
        int request_id = 0;
        bool ok = false;
        aegis::StockState state;
        std::string status;
        std::string diagnostic;
    };

    struct ValidationFutureResult
    {
        int request_id = 0;
        aegis::AlphaValidationResult validation;
        std::string status;
    };

    struct HistoryFutureResult
    {
        int request_id = 0;
        int requested_days = 0;
        std::string symbol;
        aegis::HistoricalCandlesResult result;
    };

    struct ResearchFutureResult
    {
        int request_id = 0;
        std::string symbol;
        aegis::ResearchBundleResult result;
    };

    class StockApp
    {
    public:
        void Initialize()
        {
            config_ = aegis::LoadConfig();
            const aegis::PersistentAppData app_data = aegis::LoadPersistentAppData();
            holdings_ = app_data.holdings;
            price_alerts_ = app_data.price_alerts;
            alert_events_ = app_data.alert_events;
            trade_plans_ = app_data.trade_plans;
            symbol_notes_ = app_data.symbol_notes;
            journal_entries_ = app_data.journal_entries;
            RefreshBrokerPreview();
            aegis::SetHttpUserAgent(aegis::SecCompliantUserAgent(config_));
            light_theme_ = config_.ui_light_theme;
            compact_mode_ = config_.ui_compact_mode;
            EnsureHoldingsInWatchlist();
            state_ = aegis::MakeDemoStockState();
            selected_symbol_ = state_.selected_symbol;
            SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
            SetBuffer(api_key_buffer_, sizeof(api_key_buffer_), config_.alpha_vantage_api_key);
            SetBuffer(auth_base_buffer_, sizeof(auth_base_buffer_), config_.auth_base_url);
            SetBuffer(sec_user_agent_buffer_, sizeof(sec_user_agent_buffer_), config_.sec_user_agent);
            SetBuffer(strategy_rule_buffer_, sizeof(strategy_rule_buffer_), "RSI < 45 AND CLOSE > SMA50");
            SetBuffer(compare_symbols_buffer_, sizeof(compare_symbols_buffer_), DefaultCompareSymbols());
            LoadSessionState();
            show_setup_wizard_ = config_.alpha_vantage_api_key.empty();
            ApplyActiveTheme();

            const aegis::RememberedCredentials creds = aegis::LoadRememberedCredentials();
            if (creds.ok)
            {
                SetBuffer(login_user_buffer_, sizeof(login_user_buffer_), creds.username);
                SetBuffer(login_password_buffer_, sizeof(login_password_buffer_), creds.password);
                remember_me_ = true;
            }

            const std::string launcher_user = aegis::GetEnvUtf8(L"AEGIS_USERNAME");
            const std::string launcher_cookie = aegis::GetEnvUtf8(L"AEGIS_AUTH_COOKIE");
            if (!launcher_user.empty())
                username_ = launcher_user;
            if (!launcher_cookie.empty())
            {
                cookie_header_ = launcher_cookie;
                authenticated_ = true;
            }

            BeginRefresh(true);
        }

        void Shutdown()
        {
            ++refresh_request_id_;
            ++validation_request_id_;
            ++history_request_id_;
            ++research_request_id_;
            status_ = "Waiting for provider work to finish...";
            const auto wait_for_future = [](auto& future, const char* name) {
                if (!future.valid())
                    return;
                if (future.wait_for(std::chrono::seconds(3)) != std::future_status::ready)
                {
                    aegis::AppendDiagnosticEvent({ "warning", "app", "shutdown", name, "timeout", "Provider task did not finish within 3 seconds during shutdown; waiting for safe cleanup.", "", 0, 3000, false });
                    future.wait();
                    aegis::AppendDiagnosticEvent({ "info", "app", "shutdown", name, "completed", "Slow provider task completed after shutdown timeout warning.", "", 0, 0, false });
                }
                else
                {
                    future.wait();
                }
            };
            wait_for_future(refresh_future_, "quote-refresh");
            wait_for_future(validation_future_, "api-validation");
            wait_for_future(history_future_, "history-load");
            wait_for_future(research_future_, "research-load");
            SaveSessionState();
            refresh_in_flight_ = false;
            validation_in_flight_ = false;
            history_in_flight_ = false;
            research_in_flight_ = false;
            aegis::AppendDiagnosticEvent({ "info", "app", "shutdown", "", "ok", "All async provider futures completed before exit.", "", 0, 0, false });
        }

        void Render()
        {
            PollRefresh();
            PollValidation();
            PollHistoryLoad();
            PollResearchLoad();
            MaybeAutoRefresh();
            MaybeAlertMonitor();

            ImGuiIO& io = ImGui::GetIO();
            io.FontGlobalScale = static_cast<float>(std::clamp(config_.font_scale_percent, 85, 150)) / 100.0f;
            HandleKeyboardShortcuts(io);
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Aegis Stock Investing AI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

            RenderShell();

            ImGui::End();
        }

    private:
        aegis::Config config_;
        aegis::StockState state_;
        std::future<RefreshResult> refresh_future_;
        std::future<ValidationFutureResult> validation_future_;
        std::future<HistoryFutureResult> history_future_;
        std::future<ResearchFutureResult> research_future_;
        bool refresh_in_flight_ = false;
        bool validation_in_flight_ = false;
        bool history_in_flight_ = false;
        bool research_in_flight_ = false;
        int refresh_request_id_ = 0;
        int validation_request_id_ = 0;
        int history_request_id_ = 0;
        int history_requested_days_ = 0;
        int history_in_flight_days_ = 0;
        int research_request_id_ = 0;
        std::chrono::steady_clock::time_point last_refresh_ = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point last_alert_check_{};
        std::string status_ = "Starting Aegis stock workspace...";
        std::string validation_status_ = "Key not validated this session.";
        std::string history_status_ = "Historical candles not loaded yet.";
        std::string research_status_ = "Research bundle not loaded yet.";
        std::string alert_monitor_status_ = "Alert monitor waiting for quote data.";
        std::string selected_symbol_;
        std::string history_symbol_;
        std::string history_request_symbol_;
        std::string research_symbol_;
        std::string research_request_symbol_;
        aegis::HistoricalCandlesResult history_result_;
        aegis::ResearchBundleResult research_result_;
        int selected_tab_ = 0;
        int quote_filter_ = 0;
        int signal_filter_ = 0;
        char search_buffer_[128]{};
        char symbol_buffer_[24]{};
        char watchlist_buffer_[512]{};
        char api_key_buffer_[256]{};
        char auth_base_buffer_[256]{};
        char sec_user_agent_buffer_[256]{};
        char login_user_buffer_[128]{};
        char login_password_buffer_[128]{};
        char compare_symbols_buffer_[192]{};
        bool remember_me_ = true;
        bool authenticated_ = false;
        std::string username_;
        std::string cookie_header_;
        std::vector<aegis::PortfolioHolding> holdings_;
        std::vector<aegis::PriceAlert> price_alerts_;
        std::vector<aegis::AlertEvent> alert_events_;
        std::vector<aegis::BrokerImportRow> broker_preview_;
        std::vector<aegis::TradePlan> trade_plans_;
        std::vector<aegis::SymbolNote> symbol_notes_;
        std::vector<aegis::JournalEntry> journal_entries_;
        int selected_holding_index_ = -1;
        int selected_alert_index_ = -1;
        int selected_trade_plan_index_ = -1;
        int chart_days_ = 180;
        int chart_preset_ = 0;
        int chart_aggregation_ = 0;
        int sizing_mode_ = 1;
        int selected_note_index_ = -1;
        int selected_journal_index_ = -1;
        int screen_preset_index_ = 0;
        bool show_chart_volume_ = true;
        bool show_trend_indicators_ = true;
        bool show_momentum_indicators_ = true;
        bool show_volatility_indicators_ = true;
        bool show_relative_indicators_ = true;
        bool light_theme_ = false;
        bool compact_mode_ = false;
        bool show_setup_wizard_ = false;
        char holding_symbol_buffer_[24]{};
        char holding_note_buffer_[160]{};
        char alert_symbol_buffer_[24]{};
        char alert_note_buffer_[160]{};
        char note_symbol_buffer_[24]{};
        char note_tags_buffer_[128]{};
        char note_body_buffer_[1024]{};
        char journal_symbol_buffer_[24]{};
        char journal_action_buffer_[64]{};
        char journal_reason_buffer_[256]{};
        char journal_exit_buffer_[256]{};
        char journal_tags_buffer_[128]{};
        char journal_mistakes_buffer_[256]{};
        char journal_grade_buffer_[32]{};
        char command_buffer_[128]{};
        char strategy_rule_buffer_[160]{};
        double holding_shares_ = 0.0;
        double holding_average_cost_ = 0.0;
        double alert_trigger_price_ = 0.0;
        bool alert_above_ = true;
        bool alert_enabled_ = true;
        double worksheet_entry_ = 0.0;
        double worksheet_stop_ = 0.0;
        double journal_realized_pnl_ = 0.0;
        double strategy_fee_bps_ = 1.0;
        double strategy_slippage_bps_ = 2.0;
        int cache_prune_days_ = 7;
        std::string maintenance_status_;
        std::string broker_import_status_;
        std::string report_status_;

        struct ImportIssue
        {
            std::string file;
            int row = 0;
            std::string value;
            std::string reason;
        };
        std::vector<ImportIssue> import_issues_;

        struct PortfolioTotals
        {
            double market_value = 0.0;
            double cost_basis = 0.0;
            double pnl = 0.0;
            double pnl_percent = 0.0;
        };

        struct TradePlanStats
        {
            int open = 0;
            int active = 0;
            int closed = 0;
            int target_hits = 0;
            int stop_hits = 0;
            double open_notional = 0.0;
            double mark_to_market = 0.0;
        };

        struct ExposureRow
        {
            std::string name;
            double value = 0.0;
            double percent = 0.0;
            int count = 0;
        };

        struct DataStatusSummary
        {
            std::string label = "Unknown";
            std::string state = "Review";
            std::string freshness = "--";
            std::string detail;
            ImVec4 color = V4(0.42f, 0.48f, 0.52f, 1.0f);
        };

        void RenderShell()
        {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float sidebar_w = compact_mode_ ? 214.0f : 244.0f;

            ImGui::BeginChild("sidebar", ImVec2(sidebar_w, avail.y), true, ImGuiWindowFlags_NoScrollbar);
            RenderSidebar(sidebar_w);
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("content", ImVec2(0, avail.y), false);
            RenderHeader();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            switch (selected_tab_)
            {
            case 0: RenderDashboard(); break;
            case 1: RenderWatchlist(); break;
            case 2: RenderScanner(); break;
            case 3: RenderPortfolio(); break;
            case 4: RenderResearch(); break;
            case 5: RenderChartLab(); break;
            case 6: RenderCompare(); break;
            case 7: RenderJournal(); break;
            case 8: RenderIntegrations(); break;
            case 9: RenderSettings(); break;
            default: RenderDashboard(); break;
            }

            ImGui::EndChild();
        }

        void RenderSidebar(float width)
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 logo_pos = ImGui::GetCursorScreenPos();
            draw->AddRectFilled(logo_pos, ImVec2(logo_pos.x + width - 24.0f, logo_pos.y + 72.0f), Col(0.03f, 0.08f, 0.07f, 1.0f), 8.0f);
            DrawIcon(draw, ImVec2(logo_pos.x + 30.0f, logo_pos.y + 36.0f), 34.0f, IconKind::Shield, Col(0.42f, 1.0f, 0.62f, 1.0f), 2.0f);
            draw->AddText(g_font_title, 22.0f, ImVec2(logo_pos.x + 58.0f, logo_pos.y + 16.0f), Col(0.92f, 0.98f, 0.94f), "Aegis");
            draw->AddText(g_font_bold, 13.0f, ImVec2(logo_pos.x + 59.0f, logo_pos.y + 43.0f), Col(0.62f, 0.74f, 0.70f), "Stock Investing AI");
            ImGui::Dummy(ImVec2(width - 24.0f, 84.0f));

            if (ToolbarButton("Dashboard", IconKind::Dashboard, ImVec2(width - 24.0f, 42.0f), selected_tab_ == 0)) selected_tab_ = 0;
            if (ToolbarButton("Watchlist", IconKind::Watchlist, ImVec2(width - 24.0f, 42.0f), selected_tab_ == 1)) selected_tab_ = 1;
            if (ToolbarButton("AI Scanner", IconKind::Scanner, ImVec2(width - 24.0f, 42.0f), selected_tab_ == 2)) selected_tab_ = 2;
            if (ToolbarButton("Portfolio", IconKind::Portfolio, ImVec2(width - 24.0f, 42.0f), selected_tab_ == 3)) selected_tab_ = 3;
            if (ToolbarButton("Research", IconKind::Research, ImVec2(width - 24.0f, 42.0f), selected_tab_ == 4)) selected_tab_ = 4;
            if (ToolbarButton("Chart Lab", IconKind::Chart, ImVec2(width - 24.0f, 42.0f), selected_tab_ == 5)) selected_tab_ = 5;
            if (ToolbarButton("Compare", IconKind::Compare, ImVec2(width - 24.0f, 42.0f), selected_tab_ == 6)) selected_tab_ = 6;
            if (ToolbarButton("Journal", IconKind::Journal, ImVec2(width - 24.0f, 42.0f), selected_tab_ == 7)) selected_tab_ = 7;
            if (ToolbarButton("Integrations", IconKind::Integration, ImVec2(width - 24.0f, 42.0f), selected_tab_ == 8)) selected_tab_ = 8;
            if (ToolbarButton("Settings", IconKind::Settings, ImVec2(width - 24.0f, 42.0f), selected_tab_ == 9)) selected_tab_ = 9;

            ImGui::Dummy(ImVec2(1.0f, compact_mode_ ? 6.0f : 12.0f));
            const float source_h = std::clamp(ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - 92.0f, 74.0f, 126.0f);
            ImGui::BeginChild("source_status", ImVec2(width - 24.0f, source_h), true, ImGuiWindowFlags_NoScrollbar);
            SectionTitle(state_.source_badge.c_str());
            ImGui::TextWrapped("%s", state_.market_status.c_str());
            TextMuted(state_.last_refresh_label.c_str());
            ImGui::Separator();
            const int unread_alerts = aegis::CountUnreadAlertEvents(alert_events_);
            const std::string alert_line = std::to_string(unread_alerts) + " unread alert" + (unread_alerts == 1 ? "" : "s");
            TextMuted(alert_line.c_str());
            ImGui::TextWrapped("%s", status_.c_str());
            ImGui::EndChild();

            if (ImGui::GetWindowHeight() > ImGui::GetCursorPosY() + 82.0f)
            {
                ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 84.0f);
                ImGui::BeginChild("auth_status", ImVec2(width - 24.0f, 70.0f), true, ImGuiWindowFlags_NoScrollbar);
                TextMuted("Website session");
                ImGui::PushFont(g_font_bold);
                ImGui::TextWrapped("%s", authenticated_ ? (username_.empty() ? "Signed in" : username_.c_str()) : "Local research mode");
                ImGui::PopFont();
                ImGui::EndChild();
            }
        }

        void RenderHeader()
        {
            ImGui::BeginChild("header", ImVec2(0, 74.0f), false, ImGuiWindowFlags_NoScrollbar);
            const DataStatusSummary data_status = BuildDataStatusSummary();
            ImGui::PushFont(g_font_title);
            ImGui::TextUnformatted("Aegis Stock Investing AI");
            ImGui::PopFont();
            ImGui::SameLine();
            RenderDataStatusBadge(data_status);
            TextMuted("Native C++ market research workspace");

            ImGui::SameLine(ImGui::GetWindowWidth() - 676.0f);
            ImGui::SetNextItemWidth(168.0f);
            if (ImGui::InputTextWithHint("##command", "Command: add NVDA", command_buffer_, sizeof(command_buffer_), ImGuiInputTextFlags_EnterReturnsTrue))
                RunCommandPalette();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150.0f);
            ImGui::InputTextWithHint("##search", "Search symbols", search_buffer_, sizeof(search_buffer_));
            ImGui::SameLine();
            if (SmallActionButton(refresh_in_flight_ ? "Refreshing..." : "Refresh"))
                BeginRefresh(false);
            ImGui::SameLine();
            const int unread_alerts = aegis::CountUnreadAlertEvents(alert_events_);
            const std::string alert_button = "Alerts " + std::to_string(unread_alerts);
            if (SmallActionButton(alert_button.c_str()))
                selected_tab_ = 7;
            ImGui::SameLine();
            if (SmallActionButton("Open Web"))
                aegis::OpenExternalUrl(aegis::JoinUrl(config_.auth_base_url, config_.website_path));
            ImGui::SameLine();
            if (ImGui::Checkbox("Light", &light_theme_))
            {
                PersistUiPreferences("UI theme saved.");
            }

            ImGui::SetCursorPosY(48.0f);
            ImGui::TextWrapped("%s", data_status.detail.c_str());
            ImGui::EndChild();
        }

        void RenderDataStatusBadge(const DataStatusSummary& summary)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, summary.color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(std::min(summary.color.x + 0.08f, 1.0f), std::min(summary.color.y + 0.08f, 1.0f), std::min(summary.color.z + 0.08f, 1.0f), 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, summary.color);
            ImGui::PushStyleColor(ImGuiCol_Text, V4(1.0f, 1.0f, 1.0f, 1.0f));
            const std::string label = "Data: " + summary.label;
            if (ImGui::Button(label.c_str()))
                selected_tab_ = 8;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s\n%s", summary.state.c_str(), summary.detail.c_str());
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);
        }

        void RenderDashboard()
        {
            const float metric_w = std::max(150.0f, (ImGui::GetContentRegionAvail().x - 48.0f) / 5.0f);
            for (int i = 0; i < static_cast<int>(state_.metrics.size()) && i < 5; ++i)
            {
                if (i > 0)
                    ImGui::SameLine();
                MetricCard(state_.metrics[static_cast<size_t>(i)], ImVec2(metric_w, 106.0f));
            }

            ImGui::Spacing();
            if (SmallActionButton("Export Today's Briefing"))
                ExportDailyBriefing();
            if (!report_status_.empty())
            {
                ImGui::SameLine();
                TextMuted(report_status_.c_str());
            }
            ImGui::Spacing();
            const float right_w = 410.0f;
            ImGui::BeginChild("dashboard_table", ImVec2(ImGui::GetContentRegionAvail().x - right_w - 12.0f, 0), true);
            SectionTitle("Live Watchlist", state_.market_detail.c_str());
            RenderQuoteTable("dashboard_quotes", aegis::FilterQuotes(state_, "all", search_buffer_), 9);
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("dashboard_detail", ImVec2(right_w, 0), true);
            RenderSelectedSymbol();
            ImGui::EndChild();
        }

        void RenderWatchlist()
        {
            SectionTitle("Watchlist", "Add symbols, filter sectors, and inspect quote tape.");
            ImGui::SetNextItemWidth(150.0f);
            ImGui::InputTextWithHint("##symbol_add", "Ticker", symbol_buffer_, sizeof(symbol_buffer_));
            ImGui::SameLine();
            if (SmallActionButton("Add"))
                AddSymbolFromBuffer();
            ImGui::SameLine();
            if (SmallActionButton("Remove Selected"))
                RemoveSelectedSymbol();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150.0f);
            const char* filters[] = { "All", "Gainers", "Losers", "ETFs", "Technology", "Semiconductors" };
            ImGui::Combo("##quote_filter", &quote_filter_, filters, IM_ARRAYSIZE(filters));

            ImGui::Spacing();
            const std::string filter = QuoteFilterKey();
            ImGui::BeginChild("watch_table", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.58f), true);
            RenderQuoteTable("watch_quotes", aegis::FilterQuotes(state_, filter, search_buffer_), 14);
            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::BeginChild("watch_detail", ImVec2(0, 0), true);
            RenderSelectedSymbol();
            ImGui::EndChild();
        }

        void RenderScanner()
        {
            SectionTitle("AI Scanner", "Rule-based conviction board for the current watchlist.");
            ImGui::SetNextItemWidth(170.0f);
            const char* filters[] = { "All", "Eligible", "Bullish", "Risk", "Hold", "Wait" };
            ImGui::Combo("##signal_filter", &signal_filter_, filters, IM_ARRAYSIZE(filters));
            ImGui::SameLine();
            TextMuted("Signals are research aids, not trade instructions.");

            ImGui::Spacing();
            const std::vector<int> indexes = aegis::FilterSignalIndexes(state_, SignalFilterKey(), search_buffer_);
            if (ImGui::BeginTable("signals", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 0)))
            {
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Rating", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Risk", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Stop", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Horizon", ImGuiTableColumnFlags_WidthFixed, 95.0f);
                ImGui::TableSetupColumn("Thesis");
                ImGui::TableHeadersRow();

                for (int index : indexes)
                {
                    const aegis::StockSignal& signal = state_.signals[static_cast<size_t>(index)];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(signal.symbol.c_str(), selected_symbol_ == signal.symbol, ImGuiSelectableFlags_SpanAllColumns))
                        selected_symbol_ = signal.symbol;
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(signal.rating.c_str());
                    ImGui::TableNextColumn();
                    DrawScorePill(signal.score, ImVec2(110.0f, 11.0f));
                    ImGui::SameLine();
                    ImGui::Text("%d", signal.score);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(signal.risk.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(signal.target_price).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(signal.stop_level).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(signal.horizon.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", signal.thesis.c_str());
                }
                ImGui::EndTable();
            }
        }

        void RenderPortfolio()
        {
            SectionTitle("Portfolio", "Persistent holdings, live quote marks, and paper risk sizing.");

            const PortfolioTotals totals = CalculatePortfolioTotals();
            MetricCard({"Market value", "", aegis::FormatCurrency(totals.market_value), "", "Current value of saved holdings using the loaded quote board."}, ImVec2(210.0f, 108.0f));
            ImGui::SameLine();
            MetricCard({"Cost basis", "", aegis::FormatCurrency(totals.cost_basis), "", "Saved shares multiplied by average cost."}, ImVec2(210.0f, 108.0f));
            ImGui::SameLine();
            MetricCard({"Unrealized P/L", "", aegis::FormatCurrency(totals.pnl), "", aegis::FormatPercent(totals.pnl_percent) + " across saved positions."}, ImVec2(210.0f, 108.0f));
            ImGui::SameLine();
            MetricCard({"Cash model", "", aegis::FormatCurrency(config_.portfolio_cash), "", "Paper cash available for sizing and exposure checks."}, ImVec2(210.0f, 108.0f));

            ImGui::Spacing();
            ImGui::BeginChild("holdings_editor", ImVec2(360.0f, 0), true);
            SectionTitle("Holdings Editor");
            ImGui::InputText("Symbol", holding_symbol_buffer_, sizeof(holding_symbol_buffer_));
            ImGui::InputDouble("Shares", &holding_shares_, 1.0, 10.0, "%.4f");
            ImGui::InputDouble("Avg cost", &holding_average_cost_, 1.0, 10.0, "%.2f");
            ImGui::InputTextMultiline("Note", holding_note_buffer_, sizeof(holding_note_buffer_), ImVec2(0, 70.0f));
            if (SmallActionButton(selected_holding_index_ >= 0 ? "Update Holding" : "Add Holding"))
                SaveHoldingFromEditor();
            ImGui::SameLine();
            if (SmallActionButton("Clear"))
                ClearHoldingEditor();
            if (selected_holding_index_ >= 0)
            {
                ImGui::SameLine();
                if (SmallActionButton("Remove"))
                    RemoveSelectedHolding();
            }
            ImGui::Separator();
            SectionTitle("Risk Settings");
            ImGui::InputDouble("Paper cash", &config_.portfolio_cash, 100.0, 1000.0, "%.2f");
            ImGui::InputDouble("Max position", &config_.max_position_amount, 100.0, 500.0, "%.2f");
            ImGui::SliderInt("Min conviction", &config_.min_conviction, 1, 99);
            ImGui::SliderScalar("Risk percent", ImGuiDataType_Double, &config_.max_portfolio_risk_percent, &risk_min_, &risk_max_, "%.2f%%");
            ImGui::Checkbox("Paper only mode", &config_.paper_only_mode);
            ImGui::Checkbox("Manual confirmation", &config_.require_manual_confirmation);
            if (SmallActionButton("Save Risk Settings"))
            {
                SyncConfigFromBuffers();
                if (aegis::SaveConfig(config_))
                    status_ = "Risk settings saved.";
                BeginRefresh(false);
                Audit("risk_settings_saved", "", "");
            }
            ImGui::Spacing();
            if (SmallActionButton("Export Journal Snapshot"))
                ExportJournal();
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("portfolio_detail", ImVec2(0, 0), false);
            ImGui::BeginChild("holdings_table", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.36f), true);
            RenderHoldingsTable(totals);
            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::BeginChild("trade_plans", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.46f), true);
            RenderTradePlansTable();
            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::BeginChild("risk_detail", ImVec2(0, 0), true);
            RenderSizingWorksheet();
            ImGui::EndChild();
            ImGui::EndChild();
        }

        void RenderResearch()
        {
            SectionTitle("Research Log", "Source, model, alert, and guardrail state.");
            ImGui::BeginChild("research_left", ImVec2(ImGui::GetContentRegionAvail().x * 0.50f, 0), true);
            RenderInfoTable("Research", state_.research);
            ImGui::Spacing();
            RenderInfoTable("Alerts", state_.alerts);
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("research_right", ImVec2(0, 0), true);
            RenderPriceAlertsPanel();
            ImGui::Spacing();
            RenderInfoTable("Portfolio", state_.portfolio);
            ImGui::Spacing();
            RenderInfoTable("Rules", state_.rules);
            ImGui::EndChild();
        }

        void RenderChartLab()
        {
            const aegis::StockQuote* quote = FindExactQuote(selected_symbol_);
            const aegis::StockSignal* signal = FindExactSignal(selected_symbol_);
            if (quote == nullptr || signal == nullptr)
            {
                SectionTitle("Chart Lab");
                TextMuted("Select a symbol from the dashboard, watchlist, or scanner.");
                return;
            }

            chart_days_ = std::clamp(chart_days_, 30, 1260);
            EnsureHistoryLoad(*quote);
            EnsureResearchLoad(*quote);
            std::vector<aegis::Candle> daily_candles = history_symbol_ == quote->symbol && !history_result_.candles.empty()
                ? history_result_.candles
                : aegis::BuildSyntheticCandles(*quote, std::max(365, chart_days_));
            chart_aggregation_ = std::clamp(chart_aggregation_, 0, 2);
            const aegis::CandleAggregation chart_aggregation = aegis::NormalizeCandleAggregation(chart_aggregation_);
            const std::vector<aegis::Candle> candles = aegis::AggregateCandles(daily_candles, chart_aggregation);
            const aegis::IndicatorSnapshot indicators = aegis::BuildIndicators(candles, FindExactQuote("SPY"), FindExactQuote("QQQ"));
            const std::vector<aegis::ScreenPreset> presets = aegis::BuildScreenPresets();
            chart_preset_ = std::clamp(chart_preset_, 0, std::max(0, static_cast<int>(presets.size()) - 1));
            const aegis::BacktestResult backtest = aegis::RunSignalBacktest(candles, presets.empty() ? "Momentum" : presets[static_cast<size_t>(chart_preset_)].name);
            const aegis::StrategyBacktestResult strategy_backtest = aegis::RunStrategyRuleBacktest(candles, strategy_rule_buffer_, strategy_fee_bps_, strategy_slippage_bps_);
            const bool has_research = research_symbol_ == quote->symbol && research_result_.ok;
            const aegis::FundamentalSnapshot fundamentals = has_research && !research_result_.fundamentals.symbol.empty()
                ? research_result_.fundamentals
                : aegis::BuildFundamentals(*quote);
            const std::vector<aegis::FilingItem> filings = has_research && !research_result_.filings.empty()
                ? research_result_.filings
                : aegis::BuildFilings(quote->symbol);
            const std::vector<aegis::NewsItem> news = has_research && !research_result_.news.empty()
                ? research_result_.news
                : aegis::BuildNews(quote->symbol);
            const std::vector<aegis::EarningsItem> earnings = has_research && !research_result_.earnings.empty()
                ? research_result_.earnings
                : aegis::BuildEarnings(quote->symbol);
            const aegis::OptionSnapshot options = aegis::BuildOptionSnapshot(*quote);

            SectionTitle((quote->symbol + " Chart Lab").c_str(), "Candles, indicators, backtests, filings, news, earnings, and explainable model checks.");
            ImGui::SetNextItemWidth(150.0f);
            bool chart_layout_changed = false;
            if (ImGui::SliderInt("Visible days", &chart_days_, 30, 1260))
            {
                chart_days_ = std::clamp(chart_days_, 30, 1260);
                chart_layout_changed = true;
            }
            const auto timeframe_button = [&](const char* label, int days) {
                ImGui::SameLine();
                if (SmallActionButton(label))
                {
                    chart_days_ = std::clamp(days, 30, 1260);
                    chart_layout_changed = true;
                }
            };
            timeframe_button("1M", 22);
            timeframe_button("3M", 66);
            timeframe_button("6M", 126);
            timeframe_button("YTD", CurrentYtdTradingDays());
            timeframe_button("1Y", 252);
            timeframe_button("3Y", 756);
            timeframe_button("5Y", 1260);
            ImGui::SameLine();
            TextMuted(history_status_.c_str());

            bool indicator_layout_changed = false;
            ImGui::SetNextItemWidth(120.0f);
            const char* aggregation_names[] = { "Daily", "Weekly", "Monthly" };
            if (ImGui::Combo("Interval", &chart_aggregation_, aggregation_names, IM_ARRAYSIZE(aggregation_names)))
            {
                chart_aggregation_ = std::clamp(chart_aggregation_, 0, 2);
                chart_layout_changed = true;
            }
            ImGui::SameLine();
            TextMuted((std::to_string(static_cast<int>(daily_candles.size())) + " daily / " + std::to_string(static_cast<int>(candles.size())) + " " + aegis::CandleAggregationName(chart_aggregation) + " bars").c_str());
            ImGui::SameLine();
            indicator_layout_changed |= ImGui::Checkbox("Volume", &show_chart_volume_);
            ImGui::SameLine();
            indicator_layout_changed |= ImGui::Checkbox("Trend SMA/EMA", &show_trend_indicators_);
            ImGui::SameLine();
            indicator_layout_changed |= ImGui::Checkbox("Momentum RSI/MACD", &show_momentum_indicators_);
            ImGui::SameLine();
            indicator_layout_changed |= ImGui::Checkbox("Volatility BB/ATR", &show_volatility_indicators_);
            ImGui::SameLine();
            indicator_layout_changed |= ImGui::Checkbox("Relative SPY/QQQ", &show_relative_indicators_);
            if (chart_layout_changed || indicator_layout_changed)
                SaveSessionState();
            TextMuted(research_status_.c_str());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(170.0f);
            if (!presets.empty() && ImGui::BeginCombo("Backtest rule", presets[static_cast<size_t>(chart_preset_)].name.c_str()))
            {
                for (int i = 0; i < static_cast<int>(presets.size()); ++i)
                {
                    const bool selected = chart_preset_ == i;
                    if (ImGui::Selectable(presets[static_cast<size_t>(i)].name.c_str(), selected))
                        chart_preset_ = i;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (SmallActionButton("Promote Signal to Plan"))
            {
                selected_symbol_ = quote->symbol;
                worksheet_entry_ = quote->price;
                worksheet_stop_ = signal->stop_level;
                selected_symbol_for_worksheet_ = quote->symbol;
                SaveTradePlanFromWorksheet();
            }
            ImGui::SameLine();
            if (SmallActionButton("Export HTML Report"))
                ExportResearchReport(*quote, *signal, indicators, backtest, strategy_backtest, fundamentals, filings, news, earnings);
            if (!report_status_.empty())
            {
                ImGui::SameLine();
                TextMuted(report_status_.c_str());
            }

            ImGui::Spacing();
            ImGui::BeginChild("chart_canvas_panel", ImVec2(ImGui::GetContentRegionAvail().x * 0.62f, 316.0f), true);
            PlotCandleChart(candles, chart_days_, show_chart_volume_, ImVec2(ImGui::GetContentRegionAvail().x, 286.0f));
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("chart_metrics_panel", ImVec2(0, 316.0f), true);
            DrawLabeledValue("Regime", indicators.regime, "Model confidence decays when market data is stale or volatility expands.");
            ImGui::Separator();
            if (ImGui::BeginTable("indicator_snapshot", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Indicator", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();
                const auto row = [](const char* name, const std::string& value) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(name);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(value.c_str());
                };
                bool rendered_indicator = false;
                if (show_trend_indicators_)
                {
                    rendered_indicator = true;
                    row("SMA 20", aegis::FormatCurrency(indicators.sma20));
                    row("SMA 50", aegis::FormatCurrency(indicators.sma50));
                    row("EMA 12/26", aegis::FormatCurrency(indicators.ema12) + " / " + aegis::FormatCurrency(indicators.ema26));
                }
                if (show_momentum_indicators_)
                {
                    rendered_indicator = true;
                    row("RSI 14", aegis::FormatPercent(indicators.rsi14));
                    row("MACD", aegis::FormatCurrency(indicators.macd) + " / " + aegis::FormatCurrency(indicators.macd_signal));
                }
                if (show_volatility_indicators_)
                {
                    rendered_indicator = true;
                    row("Bollinger", aegis::FormatCurrency(indicators.bollinger_lower) + " - " + aegis::FormatCurrency(indicators.bollinger_upper));
                    row("ATR 14", aegis::FormatCurrency(indicators.atr14));
                    row("Drawdown", aegis::FormatPercent(indicators.drawdown));
                }
                if (show_relative_indicators_)
                {
                    rendered_indicator = true;
                    row("Rel SPY/QQQ", aegis::FormatPercent(indicators.relative_spy) + " / " + aegis::FormatPercent(indicators.relative_qqq));
                }
                if (!rendered_indicator)
                    row("Indicators", "Disabled for this layout");
                ImGui::EndTable();
            }
            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::BeginChild("chart_lower_left", ImVec2(ImGui::GetContentRegionAvail().x * 0.50f, 0), true);
            SectionTitle("Backtest Snapshot", "Synthetic candle test of the selected signal rule.");
            MetricCard({ "Trades", "", std::to_string(backtest.trades), "", "Completed historical signal windows." }, ImVec2(158.0f, 94.0f));
            ImGui::SameLine();
            MetricCard({ "Win rate", "", aegis::FormatPercent(backtest.win_rate), "", std::to_string(backtest.wins) + " wins / " + std::to_string(backtest.losses) + " losses." }, ImVec2(158.0f, 94.0f));
            ImGui::SameLine();
            MetricCard({ "Avg return", "", aegis::FormatPercent(backtest.average_return), "", "Average return per completed signal." }, ImVec2(158.0f, 94.0f));
            ImGui::SameLine();
            MetricCard({ "Max drawdown", "", aegis::FormatPercent(backtest.max_drawdown), "", "Worst equity dip during test." }, ImVec2(158.0f, 94.0f));
            ImGui::Spacing();
            RenderInfoTable("Backtest Detail", backtest.rows);
            ImGui::Spacing();
            SectionTitle("Strategy Builder", "Rules use closed candle data and enter on the next open.");
            ImGui::InputText("Rule##strategy_builder", strategy_rule_buffer_, sizeof(strategy_rule_buffer_));
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputDouble("Fee bps", &strategy_fee_bps_, 0.5, 5.0, "%.1f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputDouble("Slippage bps", &strategy_slippage_bps_, 0.5, 5.0, "%.1f");
            ImGui::TextWrapped("%s", strategy_backtest.status.c_str());
            if (strategy_backtest.ok)
            {
                MetricCard({ "Rule trades", "", std::to_string(strategy_backtest.backtest.trades), "", "Signals that entered on the next open." }, ImVec2(158.0f, 94.0f));
                ImGui::SameLine();
                MetricCard({ "Rule win rate", "", aegis::FormatPercent(strategy_backtest.backtest.win_rate), "", "Completed rule trades." }, ImVec2(158.0f, 94.0f));
                ImGui::SameLine();
                MetricCard({ "Rule avg", "", aegis::FormatPercent(strategy_backtest.backtest.average_return), "", "Average net return after costs." }, ImVec2(158.0f, 94.0f));
                RenderInfoTable("Strategy Detail", strategy_backtest.backtest.rows);
            }

            ImGui::Spacing();
            RenderInfoTable("Explainable Score", aegis::BuildModelExplanation(*quote, *signal, indicators));
            ImGui::Spacing();
            RenderInfoTable("Risk Critic", aegis::BuildRiskCritic(*quote, *signal, indicators));
            ImGui::Spacing();
            RenderInfoTable("Similar Setups", aegis::BuildSimilarSetups(*quote, indicators));
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("chart_lower_right", ImVec2(0, 0), true);
            SectionTitle("Fundamentals", quote->fundamentals_estimated ? "Estimated until provider-backed fundamentals sync." : "Provider-backed values where available.");
            if (ImGui::BeginTable("fundamentals_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Metric");
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 115.0f);
                ImGui::TableSetupColumn("Read");
                ImGui::TableHeadersRow();
                const auto frow = [](const char* name, const std::string& value, const char* detail) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(name);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(value.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", detail);
                };
                frow("Revenue", aegis::FormatCompactCurrency(fundamentals.revenue), "Top-line scale");
                frow("EPS", aegis::FormatCurrency(fundamentals.eps), "Earnings per share");
                frow("Margins", aegis::FormatPercent(fundamentals.gross_margin) + " / " + aegis::FormatPercent(fundamentals.net_margin), "Gross / net");
                frow("Debt/Equity", aegis::FormatPercent(fundamentals.debt_to_equity), "Balance sheet leverage");
                frow("Free cash flow", aegis::FormatCompactCurrency(fundamentals.free_cash_flow), "Cash generation");
                frow("P/E / PEG", aegis::FormatPercent(fundamentals.pe) + " / " + aegis::FormatPercent(fundamentals.peg), "Valuation");
                frow("Dividend", aegis::FormatPercent(fundamentals.dividend_yield), "Income contribution");
                ImGui::EndTable();
            }
            ImGui::TextWrapped("%s", fundamentals.summary.c_str());

            ImGui::Spacing();
            RenderInfoTable("Trade Checklist", aegis::BuildTradeChecklist(*quote, *signal, indicators));
            ImGui::Spacing();
            RenderEtfExposure(quote->symbol);
            ImGui::Spacing();
            RenderFilingsNewsEarnings(options, filings, news, earnings);
            ImGui::EndChild();
        }

        void RenderCompare()
        {
            SectionTitle("Compare Mode", "Side-by-side price, signal, risk/reward, trend, and factor checks.");
            ImGui::SetNextItemWidth(330.0f);
            ImGui::InputText("Symbols##compare", compare_symbols_buffer_, sizeof(compare_symbols_buffer_));
            ImGui::SameLine();
            if (SmallActionButton("Use Watchlist Top"))
                SetBuffer(compare_symbols_buffer_, sizeof(compare_symbols_buffer_), DefaultCompareSymbols());
            ImGui::SameLine();
            if (SmallActionButton("Add Selected"))
            {
                std::vector<std::string> symbols = aegis::SplitWatchlist(compare_symbols_buffer_);
                const std::string selected = aegis::Upper(selected_symbol_);
                if (!selected.empty() && std::find(symbols.begin(), symbols.end(), selected) == symbols.end())
                    symbols.push_back(selected);
                SetBuffer(compare_symbols_buffer_, sizeof(compare_symbols_buffer_), aegis::JoinWatchlist(symbols));
            }
            ImGui::SameLine();
            if (SmallActionButton("Save Session"))
            {
                SaveSessionState();
                status_ = "Compare basket saved to session.";
            }

            std::vector<std::string> symbols = aegis::SplitWatchlist(compare_symbols_buffer_);
            if (symbols.size() > 6)
                symbols.resize(6);
            if (symbols.empty())
            {
                TextMuted("Enter two or more symbols to compare.");
                return;
            }
            SetBuffer(compare_symbols_buffer_, sizeof(compare_symbols_buffer_), aegis::JoinWatchlist(symbols));

            ImGui::Spacing();
            if (ImGui::BeginTable("compare_summary", 10, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX))
            {
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                ImGui::TableSetupColumn("Move", ImGuiTableColumnFlags_WidthFixed, 74.0f);
                ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthFixed, 102.0f);
                ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                ImGui::TableSetupColumn("Confidence", ImGuiTableColumnFlags_WidthFixed, 88.0f);
                ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                ImGui::TableSetupColumn("Stop", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                ImGui::TableSetupColumn("Risk", ImGuiTableColumnFlags_WidthFixed, 94.0f);
                ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 112.0f);
                ImGui::TableHeadersRow();
                for (const std::string& symbol : symbols)
                {
                    const aegis::StockQuote* quote = FindExactQuote(symbol);
                    const aegis::StockSignal* signal = FindExactSignal(symbol);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(symbol.c_str(), selected_symbol_ == symbol, ImGuiSelectableFlags_SpanAllColumns))
                        selected_symbol_ = symbol;
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(quote ? aegis::FormatCurrency(quote->price).c_str() : "--");
                    ImGui::TableNextColumn();
                    if (quote)
                        TextValueColored(quote->change_percent, aegis::FormatPercent(quote->change_percent));
                    else
                        ImGui::TextUnformatted("--");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(signal ? signal->rating.c_str() : "--");
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", signal ? signal->score : 0);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d%%", signal ? signal->confidence : 0);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(signal ? aegis::FormatCurrency(signal->target_price).c_str() : "--");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(signal ? aegis::FormatCurrency(signal->stop_level).c_str() : "--");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(signal ? signal->risk.c_str() : "--");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(quote ? quote->source.c_str() : "Missing");
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();
            const float card_w = std::max(240.0f, (ImGui::GetContentRegionAvail().x - 14.0f * std::max(0, static_cast<int>(symbols.size()) - 1)) / static_cast<float>(std::max<size_t>(1, symbols.size())));
            for (int i = 0; i < static_cast<int>(symbols.size()); ++i)
            {
                if (i > 0)
                    ImGui::SameLine();
                const std::string& symbol = symbols[static_cast<size_t>(i)];
                const aegis::StockQuote* quote = FindExactQuote(symbol);
                const aegis::StockSignal* signal = FindExactSignal(symbol);
                ImGui::PushID(symbol.c_str());
                ImGui::BeginChild("compare_card", ImVec2(card_w, 275.0f), true);
                if (quote == nullptr || signal == nullptr)
                {
                    SectionTitle(symbol.c_str());
                    TextMuted("Symbol is not loaded in the current quote board.");
                    ImGui::EndChild();
                    ImGui::PopID();
                    continue;
                }

                ImGui::PushFont(g_font_title);
                ImGui::TextUnformatted(quote->symbol.c_str());
                ImGui::PopFont();
                ImGui::SameLine();
                TextValueColored(quote->change_percent, aegis::FormatPercent(quote->change_percent));
                ImGui::TextUnformatted(quote->name.c_str());
                PlotHistory(quote->history, ImVec2(0, 72.0f));
                DrawScorePill(signal->score, ImVec2(ImGui::GetContentRegionAvail().x, 10.0f));
                ImGui::TextWrapped("%s", signal->posture.c_str());

                const std::vector<aegis::Candle> candles = aegis::BuildSyntheticCandles(*quote, 120);
                const aegis::IndicatorSnapshot indicators = aegis::BuildIndicators(candles, FindExactQuote("SPY"), FindExactQuote("QQQ"));
                const aegis::FundamentalSnapshot fundamentals = aegis::BuildFundamentals(*quote);
                ImGui::Separator();
                DrawLabeledValue("RSI / ATR", aegis::FormatPercent(indicators.rsi14) + " / " + aegis::FormatCurrency(indicators.atr14), indicators.regime);
                DrawLabeledValue("P/E / Dividend", aegis::FormatPercent(fundamentals.pe) + " / " + aegis::FormatPercent(fundamentals.dividend_yield), "Fundamental snapshot.");
                DrawLabeledValue("Risk/reward", aegis::FormatCurrency(signal->stop_level) + " -> " + aegis::FormatCurrency(signal->target_price), signal->risk);
                if (SmallActionButton("Open Chart"))
                {
                    selected_symbol_ = quote->symbol;
                    selected_tab_ = 5;
                }
                ImGui::EndChild();
                ImGui::PopID();
            }

            ImGui::Spacing();
            SectionTitle("Return Correlation");
            if (ImGui::BeginTable("compare_correlation", static_cast<int>(symbols.size()) + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX))
            {
                ImGui::TableSetupColumn("");
                for (const std::string& symbol : symbols)
                    ImGui::TableSetupColumn(symbol.c_str(), ImGuiTableColumnFlags_WidthFixed, 74.0f);
                ImGui::TableHeadersRow();
                for (const std::string& row_symbol : symbols)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row_symbol.c_str());
                    for (const std::string& col_symbol : symbols)
                    {
                        const double corr = row_symbol == col_symbol ? 1.0 : ReturnCorrelation(FindExactQuote(row_symbol), FindExactQuote(col_symbol));
                        ImGui::TableNextColumn();
                        TextValueColored(corr >= 0.0 ? 1.0 : -1.0, std::to_string(static_cast<int>(corr * 100.0) / 100.0));
                    }
                }
                ImGui::EndTable();
            }
        }

        void RenderJournal()
        {
            SectionTitle("Journal & Workflow", "Notes, focus list, screen presets, watchlist groups, alert ledger, and trade review.");
            ImGui::BeginChild("workflow_left", ImVec2(ImGui::GetContentRegionAvail().x * 0.47f, 0), true);
            RenderTodayFocus();
            ImGui::Spacing();
            RenderNotificationCenter();
            ImGui::Spacing();
            RenderWatchlistGroupsPanel();
            ImGui::Spacing();
            RenderScreenPresetsPanel();
            ImGui::Spacing();
            RenderAlertHistoryPanel();
            ImGui::Spacing();
            RenderConvictionHeatmap();
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("workflow_right", ImVec2(0, 0), true);
            RenderSymbolNotesPanel();
            ImGui::Spacing();
            RenderJournalPanel();
            ImGui::Spacing();
            SectionTitle("Import / Export");
            if (SmallActionButton("Export CSV Pack"))
                ExportWorkflowCsv();
            ImGui::SameLine();
            if (SmallActionButton("Import CSV Pack"))
                ImportWorkflowCsv();
            ImGui::SameLine();
            if (SmallActionButton("Backup All Data"))
                ExportAllData();
            ImGui::TextWrapped("CSV pack uses holdings, alerts, alert-events, trade-plans, symbol-notes, and trade-journal CSV files in the app data folder.");
            ImGui::Spacing();
            RenderInfoTable("Import Preview", BuildImportPreviewRows());
            if (!import_issues_.empty())
            {
                ImGui::Spacing();
                SectionTitle("Last Import Issues");
                if (ImGui::BeginTable("ImportIssues", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn("File");
                    ImGui::TableSetupColumn("Row");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableSetupColumn("Reason");
                    ImGui::TableHeadersRow();
                    for (const ImportIssue& issue : import_issues_)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(issue.file.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%d", issue.row);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextWrapped("%s", issue.value.c_str());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextWrapped("%s", issue.reason.c_str());
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::Spacing();
            RenderBrokerImportPanel();
            ImGui::EndChild();
        }

        void RenderIntegrations()
        {
            SectionTitle("Integrations, Risk & Safety", "Provider health, macro context, portfolio risk, paper trading guardrails, and diagnostics.");
            if (show_setup_wizard_)
            {
                ImGui::BeginChild("setup_wizard", ImVec2(0, 140.0f), true);
                SectionTitle("First-run Setup");
                ImGui::InputText("Alpha Vantage key##wizard", api_key_buffer_, sizeof(api_key_buffer_), ImGuiInputTextFlags_Password);
                ImGui::InputTextMultiline("Starter watchlist##wizard", watchlist_buffer_, sizeof(watchlist_buffer_), ImVec2(0, 48.0f));
                if (SmallActionButton("Save Setup"))
                {
                    SyncConfigFromBuffers();
                    aegis::SaveConfig(config_);
                    show_setup_wizard_ = false;
                    BeginRefresh(false);
                    Audit("first_run_setup_saved", "", "");
                }
                ImGui::SameLine();
                if (SmallActionButton("Skip"))
                    show_setup_wizard_ = false;
                ImGui::EndChild();
                ImGui::Spacing();
            }

            ImGui::BeginChild("integration_left", ImVec2(ImGui::GetContentRegionAvail().x * 0.52f, 0), true);
            SectionTitle("Provider Layer");
            const std::vector<aegis::ProviderStatus> providers = aegis::BuildProviderStatuses(config_);
            if (ImGui::BeginTable("provider_status", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("Provider", ImGuiTableColumnFlags_WidthFixed, 130.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Detail");
                ImGui::TableSetupColumn("Link", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                ImGui::TableHeadersRow();
                for (const aegis::ProviderStatus& provider : providers)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(provider.name.c_str());
                    ImGui::TableNextColumn();
                    TextValueColored(provider.enabled ? 1.0 : -1.0, provider.status);
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", provider.detail.c_str());
                    ImGui::TableNextColumn();
                    ImGui::PushID(provider.name.c_str());
                    if (!provider.url.empty() && SmallActionButton("Open"))
                        aegis::OpenExternalUrl(provider.url);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
            RenderInfoTable("Provider Controls", aegis::BuildProviderControlRows(config_));
            ImGui::Spacing();
            RenderInfoTable("Provider Capability Registry", aegis::BuildProviderCapabilityRows(config_));
            ImGui::Spacing();
            RenderInfoTable("Cache Policy", aegis::BuildCachePolicyRows(config_));
            ImGui::Spacing();
            RenderInfoTable("Recent Provider Diagnostics", aegis::BuildProviderDiagnosticRows(6));
            ImGui::Spacing();
            RenderInfoTable("Rate-Limit Events", aegis::BuildProviderLimitRows(32));
            ImGui::Spacing();
            SectionTitle("Macro Dashboard");
            if (ImGui::BeginTable("macro_dashboard", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Metric");
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Trend", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                ImGui::TableSetupColumn("Read");
                ImGui::TableHeadersRow();
                for (const aegis::MacroItem& item : aegis::BuildMacroDashboard())
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(item.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(item.value.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(item.trend.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", item.detail.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
            RenderCorrelationMatrix();
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("integration_right", ImVec2(0, 0), true);
            RenderRiskUpgradePanel();
            ImGui::Spacing();
            SectionTitle("Paper Trading Guardrails");
            ImGui::Checkbox("Paper only mode", &config_.paper_only_mode);
            ImGui::Checkbox("Manual confirmation required", &config_.require_manual_confirmation);
            ImGui::Checkbox("Desktop notifications", &config_.notifications_enabled);
            if (ImGui::Checkbox("Compact dashboard", &compact_mode_))
                PersistUiPreferences("Compact dashboard preference saved.");
            ImGui::TextWrapped("Live order execution is disabled unless a future broker module explicitly enables it. Alpaca support is shaped for paper accounts first.");
            if (SmallActionButton("Save Guardrails"))
            {
                SyncConfigFromBuffers();
                aegis::SaveConfig(config_);
                status_ = "Guardrails saved.";
                Audit("guardrails_saved", "", "");
            }
            ImGui::SameLine();
            if (SmallActionButton(validation_in_flight_ ? "Checking..." : "API Health Check"))
                BeginValidation();
            ImGui::TextWrapped("%s", validation_status_.c_str());
            ImGui::Separator();
            SectionTitle("Data Status");
            const DataStatusSummary data_status = BuildDataStatusSummary();
            DrawLabeledValue("Mode", data_status.label, data_status.detail);
            RenderInfoTable("Data Status Breakdown", BuildDataStatusRows(data_status));
            DrawLabeledValue("Freshness", state_.last_refresh_label, state_.market_detail);
            DrawLabeledValue("Chart source", history_result_.source.empty() ? "Not loaded" : history_result_.source, history_result_.cache_age_label.empty() ? history_status_ : history_result_.cache_age_label);
            DrawLabeledValue("Research source", research_result_.source.empty() ? "Not loaded" : research_result_.source, research_result_.cache_age_label.empty() ? research_status_ : research_result_.cache_age_label);
            if (!history_result_.fallback_reason.empty())
                DrawLabeledValue("History fallback", history_result_.fallback_reason, "Fallback reason from latest chart load.");
            if (!research_result_.fallback_reason.empty())
                DrawLabeledValue("Research fallback", research_result_.fallback_reason, "Fallback reason from latest research load.");
            DrawLabeledValue("Diagnostics", (aegis::AppDataDirectory() / "diagnostics.jsonl").string(), "Structured provider, rate-limit, cache, and error events.");
            ImGui::Spacing();
            RenderInfoTable("Background Tasks", BuildBackgroundTaskRows());
            ImGui::Spacing();
            RenderInfoTable("Storage Counts", aegis::BuildPersistentDataRows(CurrentPersistentData()));
            ImGui::Separator();
            SectionTitle("Maintenance");
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputInt("Prune days", &cache_prune_days_);
            cache_prune_days_ = std::clamp(cache_prune_days_, 1, 365);
            if (SmallActionButton("Prune Cache"))
                aegis::PruneProviderCaches(cache_prune_days_, maintenance_status_);
            ImGui::SameLine();
            if (SmallActionButton("Reset Demo Cache"))
                aegis::ResetDemoCaches(maintenance_status_);
            if (SmallActionButton("Validate Backup"))
            {
                const aegis::BackupValidationResult validation = aegis::ValidateBackupDirectory(aegis::AppDataDirectory());
                maintenance_status_ = validation.summary;
                if (!validation.issues.empty())
                    maintenance_status_ += " " + validation.issues.front();
            }
            ImGui::SameLine();
            if (SmallActionButton("Export Diagnostics"))
                aegis::ExportDiagnosticsSnapshot(aegis::AppDataDirectory() / "diagnostics-export", maintenance_status_);
            if (!maintenance_status_.empty())
                ImGui::TextWrapped("%s", maintenance_status_.c_str());
            if (SmallActionButton("Backup / Export All"))
                ExportAllData();
            ImGui::SameLine();
            if (SmallActionButton("Open Data Folder"))
                aegis::OpenExternalUrl(aegis::AppDataDirectory().string());
            ImGui::EndChild();
        }

        void RenderSettings()
        {
            SectionTitle("Settings", "Data, account bridge, and application preferences.");
            ImGui::BeginChild("settings_data", ImVec2(ImGui::GetContentRegionAvail().x * 0.52f, 0), true);
            SectionTitle("Market Data");
            ImGui::InputText("Alpha Vantage key", api_key_buffer_, sizeof(api_key_buffer_), ImGuiInputTextFlags_Password);
            ImGui::InputTextMultiline("Watchlist", watchlist_buffer_, sizeof(watchlist_buffer_), ImVec2(0, 88.0f));
            ImGui::SliderInt("Refresh seconds", &config_.refresh_seconds, 30, 900);
            ImGui::SliderInt("Max symbols", &config_.max_symbols, 1, 50);
            ImGui::SliderInt("Model count", &config_.model_count, 2, 32);
            ImGui::InputText("SEC user agent/contact", sec_user_agent_buffer_, sizeof(sec_user_agent_buffer_));
            ImGui::SliderInt("Quote TTL sec", &config_.alpha_quote_ttl_seconds, 15, 900);
            ImGui::SliderInt("History cache hr", &config_.history_cache_hours, 1, 720);
            ImGui::SliderInt("Research cache hr", &config_.research_cache_hours, 1, 720);
            ImGui::SliderInt("Max cache MB", &config_.max_cache_mb, 25, 2048);
            ImGui::Checkbox("Force live refresh", &config_.force_live_refresh);
            ImGui::Checkbox("Notifications", &config_.notifications_enabled);
            if (SmallActionButton("Save Settings"))
            {
                SyncConfigFromBuffers();
                if (aegis::SaveConfig(config_))
                {
                    const aegis::WatchlistLimitResult limits = aegis::ValidateWatchlistLimits(config_);
                    status_ = limits.within_limit ? "Settings saved." : "Settings saved. " + limits.detail;
                }
                BeginRefresh(false);
                Audit("settings_saved", "", "");
            }
            ImGui::SameLine();
            if (SmallActionButton(validation_in_flight_ ? "Validating..." : "Validate Key"))
                BeginValidation();
            ImGui::TextWrapped("%s", validation_status_.c_str());
            ImGui::Separator();
            TextMuted("Alpha Vantage GLOBAL_QUOTE is used for direct quotes; plan freshness depends on the API key entitlement.");
            ImGui::Spacing();
            RenderInfoTable("Storage Plan", aegis::BuildStorageMigrationRows());
            ImGui::Spacing();
            RenderInfoTable("Settings Health", aegis::BuildSettingsHealthRows(config_));
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("settings_auth", ImVec2(0, 0), true);
            SectionTitle("Website Auth Bridge");
            ImGui::InputText("Auth base URL", auth_base_buffer_, sizeof(auth_base_buffer_));
            ImGui::InputText("Username", login_user_buffer_, sizeof(login_user_buffer_));
            ImGui::InputText("Password", login_password_buffer_, sizeof(login_password_buffer_), ImGuiInputTextFlags_Password);
            ImGui::Checkbox("Remember credentials", &remember_me_);
            if (ImGui::Checkbox("Compact dashboard", &compact_mode_))
                PersistUiPreferences("Compact dashboard preference saved.");
            if (ImGui::Checkbox("High contrast mode", &config_.ui_high_contrast))
                PersistUiPreferences("High contrast preference saved.");
            if (ImGui::SliderInt("Font scale", &config_.font_scale_percent, 85, 150))
                PersistUiPreferences("Font scale preference saved.");
            if (SmallActionButton("Sign In"))
                SignIn(true);
            ImGui::SameLine();
            if (SmallActionButton("Clear Remembered"))
            {
                aegis::DeleteRememberedCredentials();
                SetBuffer(login_password_buffer_, sizeof(login_password_buffer_), "");
                status_ = "Remembered credentials cleared.";
            }
            ImGui::SameLine();
            if (SmallActionButton("Clear All Secrets"))
            {
                const aegis::SecretClearResult result = aegis::ClearStoredSecrets(config_);
                config_ = result.config;
                SetBuffer(api_key_buffer_, sizeof(api_key_buffer_), "");
                SetBuffer(login_password_buffer_, sizeof(login_password_buffer_), "");
                status_ = result.status;
                Audit("clear_all_secrets", "", "");
            }
            ImGui::Spacing();
            ImGui::TextWrapped("%s", authenticated_ ? "Website bridge connected for this session." : "Desktop market research works locally even when the website bridge is not signed in.");
            ImGui::Separator();
            SectionTitle("Diagnostics");
            ImGui::TextWrapped("Structured logs: %s", (aegis::AppDataDirectory() / "diagnostics.jsonl").string().c_str());
            if (SmallActionButton("Open Diagnostics Folder"))
                aegis::OpenExternalUrl(aegis::AppDataDirectory().string());
            ImGui::SameLine();
            if (SmallActionButton("Run Self-Test"))
            {
                const int code = aegis::RunSelfTests();
                status_ = code == 0 ? "Self-test passed." : "Self-test failed. See self-test-report.txt.";
            }
            ImGui::Spacing();
            RenderInfoTable("Production Readiness", aegis::BuildProductionReadinessRows(config_));
            ImGui::Spacing();
            RenderInfoTable("Keyboard And Commands", BuildShortcutHelpRows());
            ImGui::Spacing();
            RenderInfoTable("Background Tasks", BuildBackgroundTaskRows());
            ImGui::Spacing();
            const std::vector<std::string> recent = aegis::LoadRecentDiagnosticLines(4);
            if (!recent.empty())
            {
                SectionTitle("Recent Diagnostics");
                for (const std::string& line : recent)
                    ImGui::TextWrapped("%s", line.c_str());
            }
            ImGui::EndChild();
        }

        void RenderQuoteTable(const char* id, const std::vector<const aegis::StockQuote*>& quotes, int max_rows)
        {
            if (ImGui::BeginTable(id, 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 0)))
            {
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableSetupColumn("Company", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 96.0f);
                ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthFixed, 112.0f);
                ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 112.0f);
                ImGui::TableHeadersRow();

                const int rows = std::min(static_cast<int>(quotes.size()), max_rows);
                for (int i = 0; i < rows; ++i)
                {
                    const aegis::StockQuote* quote = quotes[static_cast<size_t>(i)];
                    const aegis::StockSignal* signal = aegis::FindSignal(state_, quote->symbol);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(quote->symbol.c_str(), selected_symbol_ == quote->symbol, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        selected_symbol_ = quote->symbol;
                        worksheet_entry_ = 0.0;
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(quote->name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(quote->price).c_str());
                    ImGui::TableNextColumn();
                    TextValueColored(quote->change_percent, aegis::FormatPercent(quote->change_percent));
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatVolume(quote->volume).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(signal ? signal->rating.c_str() : "--");
                    ImGui::TableNextColumn();
                    if (signal)
                    {
                        DrawScorePill(signal->score, ImVec2(82.0f, 10.0f));
                        ImGui::SameLine();
                        ImGui::Text("%d", signal->score);
                    }
                    else
                    {
                        ImGui::TextUnformatted("--");
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(quote->source.c_str());
                }
                ImGui::EndTable();
            }
        }

        void RenderSelectedSymbol()
        {
            const aegis::StockQuote* quote = aegis::FindQuote(state_, selected_symbol_);
            const aegis::StockSignal* signal = aegis::FindSignal(state_, selected_symbol_);
            if (quote == nullptr || signal == nullptr)
            {
                TextMuted("No symbol selected.");
                return;
            }

            ImGui::PushFont(g_font_title);
            ImGui::Text("%s", quote->symbol.c_str());
            ImGui::PopFont();
            ImGui::SameLine();
            TextMuted(quote->name.c_str());
            ImGui::Spacing();

            ImGui::PushFont(g_font_title);
            ImGui::TextUnformatted(aegis::FormatCurrency(quote->price).c_str());
            ImGui::PopFont();
            ImGui::SameLine();
            TextValueColored(quote->change_percent, aegis::FormatPercent(quote->change_percent));
            ImGui::SameLine();
            TextMuted(quote->latest_trading_day.c_str());

            PlotHistory(quote->history, ImVec2(0, 126.0f));
            ImGui::Spacing();
            SectionTitle(signal->rating.c_str(), signal->posture.c_str());
            DrawScorePill(signal->score, ImVec2(ImGui::GetContentRegionAvail().x, 12.0f));
            ImGui::TextWrapped("%s", signal->thesis.c_str());
            ImGui::Spacing();

            if (ImGui::BeginTable("selected_metrics", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Open");
                ImGui::TableSetupColumn("Range");
                ImGui::TableSetupColumn("Volume");
                ImGui::TableSetupColumn("Market Cap");
                ImGui::TableHeadersRow();
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(aegis::FormatCurrency(quote->open).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s / %s", aegis::FormatCurrency(quote->low).c_str(), aegis::FormatCurrency(quote->high).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(aegis::FormatVolume(quote->volume).c_str());
                ImGui::TableNextColumn();
                std::string market_cap = aegis::FormatCompactCurrency(quote->market_cap);
                if (quote->market_cap_estimated)
                    market_cap += " est.";
                ImGui::TextUnformatted(market_cap.c_str());
                ImGui::EndTable();
            }

            ImGui::Spacing();
            RenderInfoTable("Signal Factors", signal->factors);
            ImGui::Spacing();
            RenderInfoTable("Data Quality", aegis::BuildQuoteDataQualityRows(*quote));
        }

        void RenderInfoTable(const char* title, const std::vector<aegis::InfoItem>& items)
        {
            SectionTitle(title);
            if (ImGui::BeginTable(title, 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Detail");
                ImGui::TableHeadersRow();
                for (const aegis::InfoItem& item : items)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(item.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(!item.label.empty() ? item.label.c_str() : (!item.tag.empty() ? item.tag.c_str() : item.state.c_str()));
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(item.value.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", item.detail.c_str());
                }
                ImGui::EndTable();
            }
        }

        std::vector<aegis::InfoItem> BuildBackgroundTaskRows() const
        {
            const auto row = [](const std::string& name, bool active, int request_id, const std::string& symbol, const std::string& status) {
                aegis::InfoItem item;
                item.name = name;
                item.state = active ? "Running" : "Idle";
                item.value = "#" + std::to_string(std::max(0, request_id));
                item.detail = symbol.empty() ? status : symbol + ": " + status;
                return item;
            };
            return {
                row("Quote refresh", refresh_in_flight_, refresh_request_id_, "", status_),
                row("API validation", validation_in_flight_, validation_request_id_, "", validation_status_),
                row("History load", history_in_flight_, history_request_id_, history_request_symbol_, history_status_),
                row("Research load", research_in_flight_, research_request_id_, research_request_symbol_, research_status_)
            };
        }

        DataStatusSummary BuildDataStatusSummary() const
        {
            DataStatusSummary summary;
            summary.freshness = state_.last_refresh_label.empty() ? "--" : state_.last_refresh_label;

            const std::string combined = aegis::Lower(
                state_.source_badge + " " +
                state_.source_label + " " +
                state_.market_status + " " +
                state_.market_detail + " " +
                status_ + " " +
                validation_status_ + " " +
                history_status_ + " " +
                history_result_.source + " " +
                history_result_.cache_age_label + " " +
                history_result_.fallback_reason + " " +
                research_status_ + " " +
                research_result_.source + " " +
                research_result_.cache_age_label + " " +
                research_result_.fallback_reason);

            const auto contains = [&](const std::string& needle) {
                return combined.find(needle) != std::string::npos;
            };
            const bool has_rate_limit = contains("rate-limit") || contains("rate limit") || contains("api call frequency") || contains("provider-limit") || contains("limited");
            const bool has_network_issue = contains("network error") || contains("timeout") || contains("could not connect") || contains("offline");
            const bool demo = contains("demo") || state_.source_badge.find("Demo") != std::string::npos;
            const bool cache = contains("cache") || history_result_.cached || research_result_.cached;
            const bool stale = contains("stale") || contains("fallback") || !history_result_.fallback_reason.empty() || !research_result_.fallback_reason.empty();
            const bool quote_live = state_.loaded_from_api && !demo && !state_.quotes.empty();

            if (has_rate_limit)
            {
                summary.label = "Rate-limited";
                summary.state = "Provider limit";
                summary.color = V4(0.78f, 0.46f, 0.10f, 1.0f);
                summary.detail = "A provider limit was detected. Aegis is using cache, fallback, or partial data where available.";
            }
            else if (has_network_issue)
            {
                summary.label = "Offline";
                summary.state = "Network issue";
                summary.color = V4(0.68f, 0.18f, 0.18f, 1.0f);
                summary.detail = "Network/provider connectivity looks impaired. Decision panels should be treated as cache-only or demo until refresh succeeds.";
            }
            else if (demo)
            {
                summary.label = "Demo";
                summary.state = "Research sample";
                summary.color = V4(0.44f, 0.36f, 0.66f, 1.0f);
                summary.detail = state_.source_label.empty() ? "Demo data is active." : state_.source_label;
            }
            else if (quote_live && (cache || stale))
            {
                summary.label = stale ? "Mixed stale" : "Mixed cache";
                summary.state = stale ? "Live + fallback" : "Live + cache";
                summary.color = stale ? V4(0.76f, 0.55f, 0.16f, 1.0f) : V4(0.14f, 0.48f, 0.62f, 1.0f);
                summary.detail = "Quotes are live, while one or more research/chart panels are using cached or fallback data.";
            }
            else if (cache || stale)
            {
                summary.label = stale ? "Stale" : "Cache-only";
                summary.state = stale ? "Fallback" : "Cache";
                summary.color = stale ? V4(0.76f, 0.55f, 0.16f, 1.0f) : V4(0.14f, 0.48f, 0.62f, 1.0f);
                summary.detail = stale ? "The current workspace is using stale or fallback provider data." : "The current workspace is using cached provider data.";
            }
            else if (quote_live)
            {
                summary.label = "Live";
                summary.state = "Provider-backed";
                summary.color = V4(0.08f, 0.48f, 0.24f, 1.0f);
                summary.detail = state_.source_label.empty() ? "Live provider quote refresh is active." : state_.source_label;
            }
            else
            {
                summary.label = "Unknown";
                summary.state = "Review";
                summary.color = V4(0.42f, 0.48f, 0.52f, 1.0f);
                summary.detail = "Data status has not been established yet. Refresh or check provider diagnostics.";
            }

            return summary;
        }

        std::vector<aegis::InfoItem> BuildDataStatusRows(const DataStatusSummary& summary) const
        {
            std::vector<aegis::InfoItem> rows = {
                { "Workspace mode", summary.state, summary.label, "", summary.detail, "status", summary.state, state_.source_badge, summary.freshness },
                { "Quote board", state_.loaded_from_api ? "Live attempt" : "Fallback", state_.source_badge, "", state_.source_label, "quotes", state_.loaded_from_api ? "Ready" : "Review", state_.source_badge, state_.last_refresh_label },
                { "Market venue", state_.market_status.empty() ? "Unknown" : state_.market_status, summary.freshness, "", state_.market_detail, "market", state_.market_status.empty() ? "Review" : "Ready", state_.source_badge, state_.last_refresh_label },
                { "Chart cache", history_result_.cached ? "Cached" : (history_result_.live ? "Live" : "Not loaded"), history_result_.cache_age_label.empty() ? history_status_ : history_result_.cache_age_label, "", history_result_.fallback_reason.empty() ? "No chart fallback reason from latest load." : history_result_.fallback_reason, "history", history_result_.fallback_reason.empty() ? "Ready" : "Review", history_result_.source, history_result_.fetched_at },
                { "Research cache", research_result_.cached ? "Cached" : (research_result_.live ? "Live" : "Not loaded"), research_result_.cache_age_label.empty() ? research_status_ : research_result_.cache_age_label, "", research_result_.fallback_reason.empty() ? "No research fallback reason from latest load." : research_result_.fallback_reason, "research", research_result_.fallback_reason.empty() ? "Ready" : "Review", research_result_.source, research_result_.fetched_at }
            };

            const std::vector<aegis::InfoItem> limits = aegis::BuildProviderLimitRows(1);
            if (!limits.empty())
            {
                aegis::InfoItem row = limits.front();
                row.name = "Recent provider limit";
                row.detail = row.detail.empty() ? "Most recent rate-limit diagnostic." : row.detail;
                rows.push_back(std::move(row));
            }
            return rows;
        }

        std::vector<aegis::InfoItem> BuildShortcutHelpRows() const
        {
            return {
                { "Ctrl+R", "Refresh", "Global", "", "Refresh quote data using the current provider/cache policy.", "keyboard", "Ready" },
                { "Ctrl+A", "Alert draft", "Research", "", "Jump to Research and prefill the alert editor with the selected symbol.", "keyboard", "Ready" },
                { "Ctrl+1..9", "Switch tab", "Navigation", "", "Open Dashboard through Integrations without leaving the keyboard.", "keyboard", "Ready" },
                { "Ctrl+0", "Settings", "Navigation", "", "Open Settings.", "keyboard", "Ready" },
                { "refresh", "Command", "Palette", "", "Refresh the board from the command box.", "command", "Ready" },
                { "search MSFT", "Command", "Palette", "", "Select and add a ticker from the command box.", "command", "Ready" },
                { "alert NVDA above 900", "Command", "Palette", "", "Create a saved price alert from the command box.", "command", "Ready" },
                { "report / briefing", "Command", "Palette", "", "Export the selected research report or today's briefing.", "command", "Ready" }
            };
        }

        void RenderEtfExposure(const std::string& symbol)
        {
            SectionTitle("ETF Exposure");
            if (ImGui::BeginTable("etf_exposure", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("ETF", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableSetupColumn("Read");
                ImGui::TableHeadersRow();
                for (const aegis::EtfExposure& row : aegis::BuildEtfExposure(symbol))
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row.etf.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatPercent(row.weight).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", row.note.c_str());
                }
                ImGui::EndTable();
            }
        }

        void RenderFilingsNewsEarnings(const aegis::OptionSnapshot& options, const std::vector<aegis::FilingItem>& filing_items, const std::vector<aegis::NewsItem>& news_items, const std::vector<aegis::EarningsItem>& earnings_items)
        {
            if (!ImGui::BeginTabBar("symbol_research_tabs"))
                return;

            if (ImGui::BeginTabItem("SEC"))
            {
                if (ImGui::BeginTable("filings_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn("Form", ImGuiTableColumnFlags_WidthFixed, 56.0f);
                    ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                    ImGui::TableSetupColumn("Summary");
                    ImGui::TableSetupColumn("Risk", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                    ImGui::TableSetupColumn("Link", ImGuiTableColumnFlags_WidthFixed, 64.0f);
                    ImGui::TableHeadersRow();
                    for (const aegis::FilingItem& filing : filing_items)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(filing.form.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(filing.date.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s", filing.summary.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s", filing.risk_change.c_str());
                        ImGui::TableNextColumn();
                        ImGui::PushID((filing.form + filing.date).c_str());
                        if (SmallActionButton("Open"))
                            aegis::OpenExternalUrl(filing.url);
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("News"))
            {
                if (ImGui::BeginTable("news_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                    ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                    ImGui::TableSetupColumn("Sentiment", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                    ImGui::TableSetupColumn("Headline");
                    ImGui::TableSetupColumn("Link", ImGuiTableColumnFlags_WidthFixed, 64.0f);
                    ImGui::TableHeadersRow();
                    for (const aegis::NewsItem& item : news_items)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(item.date.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(item.source.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(item.sentiment.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s - %s", item.title.c_str(), item.summary.c_str());
                        ImGui::TableNextColumn();
                        ImGui::PushID(item.title.c_str());
                        if (SmallActionButton("Open"))
                            aegis::OpenExternalUrl(item.url);
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Earnings"))
            {
                if (ImGui::BeginTable("earnings_table", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Date");
                    ImGui::TableSetupColumn("State");
                    ImGui::TableSetupColumn("Estimate");
                    ImGui::TableSetupColumn("Actual");
                    ImGui::TableSetupColumn("Surprise");
                    ImGui::TableSetupColumn("Move / Drift");
                    ImGui::TableHeadersRow();
                    for (const aegis::EarningsItem& item : earnings_items)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(item.date.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(item.status.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(aegis::FormatCurrency(item.eps_estimate).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(item.eps_actual > 0.0 ? aegis::FormatCurrency(item.eps_actual).c_str() : "--");
                        ImGui::TableNextColumn();
                        TextValueColored(item.surprise, aegis::FormatPercent(item.surprise));
                        ImGui::TableNextColumn();
                        ImGui::Text("%s / %s", aegis::FormatPercent(item.estimated_move).c_str(), aegis::FormatPercent(item.drift).c_str());
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Options"))
            {
                DrawLabeledValue("Implied volatility", aegis::FormatPercent(options.implied_volatility), "Used for expected move and volatility-adjusted sizing.");
                DrawLabeledValue("Expected move", aegis::FormatPercent(options.expected_move), "Provider-ready estimate until an options chain account is configured.");
                DrawLabeledValue("Put/call + skew", aegis::FormatPercent(options.put_call_ratio) + " / " + aegis::FormatPercent(options.skew), options.unusual_flow);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        void RenderTodayFocus()
        {
            SectionTitle("Today's Focus", "Symbols with price movement, alert pressure, signal changes, earnings, or news.");
            const std::vector<aegis::FocusItem> focus = aegis::BuildFocusItems(state_, price_alerts_);
            if (ImGui::BeginTable("today_focus", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 78.0f);
                ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Reason", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Detail");
                ImGui::TableHeadersRow();
                for (const aegis::FocusItem& item : focus)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(item.symbol.c_str(), selected_symbol_ == item.symbol, ImGuiSelectableFlags_SpanAllColumns))
                        selected_symbol_ = item.symbol;
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(item.priority.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(item.reason.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", item.detail.c_str());
                }
                ImGui::EndTable();
            }
        }

        void RenderWatchlistGroupsPanel()
        {
            SectionTitle("Watchlist Groups");
            const std::vector<aegis::WatchlistGroup> groups = aegis::BuildWatchlistGroups(aegis::SplitWatchlist(config_.watchlist));
            if (ImGui::BeginTable("watchlist_groups", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Symbols");
                ImGui::TableHeadersRow();
                for (const aegis::WatchlistGroup& group : groups)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(group.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", static_cast<int>(group.symbols.size()));
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", aegis::JoinWatchlist(group.symbols).c_str());
                }
                ImGui::EndTable();
            }
        }

        void RenderScreenPresetsPanel()
        {
            SectionTitle("Saved Screen Presets");
            const std::vector<aegis::ScreenPreset> presets = aegis::BuildScreenPresets();
            if (ImGui::BeginTable("screen_presets", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Preset", ImGuiTableColumnFlags_WidthFixed, 118.0f);
                ImGui::TableSetupColumn("Filter", ImGuiTableColumnFlags_WidthFixed, 104.0f);
                ImGui::TableSetupColumn("Read");
                ImGui::TableHeadersRow();
                for (int i = 0; i < static_cast<int>(presets.size()); ++i)
                {
                    const aegis::ScreenPreset& preset = presets[static_cast<size_t>(i)];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(preset.name.c_str(), screen_preset_index_ == i, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        screen_preset_index_ = i;
                        selected_tab_ = 2;
                        signal_filter_ = preset.filter_key == "risk" ? 3 : preset.filter_key == "hold" ? 4 : preset.filter_key == "wait" ? 5 : 1;
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(preset.filter_key.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", preset.description.c_str());
                }
                ImGui::EndTable();
            }
        }

        void RenderAlertHistoryPanel()
        {
            SectionTitle("Alert Engine", "Persistent trigger history with acknowledge and snooze controls.");
            const int unread_alerts = aegis::CountUnreadAlertEvents(alert_events_);
            ImGui::TextWrapped("%d unread alert%s. %s", unread_alerts, unread_alerts == 1 ? "" : "s", alert_monitor_status_.c_str());
            if (ImGui::BeginTable("alert_events", 10, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 220.0f)))
            {
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 76.0f);
                ImGui::TableSetupColumn("Rule", ImGuiTableColumnFlags_WidthFixed, 104.0f);
                ImGui::TableSetupColumn("Observed", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableSetupColumn("Since", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Triggered", ImGuiTableColumnFlags_WidthFixed, 118.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Snooze", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 164.0f);
                ImGui::TableSetupColumn("Note");
                ImGui::TableHeadersRow();
                for (int i = static_cast<int>(alert_events_.size()) - 1; i >= 0; --i)
                {
                    aegis::AlertEvent& event = alert_events_[static_cast<size_t>(i)];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(event.symbol.c_str(), selected_symbol_ == event.symbol, ImGuiSelectableFlags_SpanAllColumns))
                        selected_symbol_ = event.symbol;
                    ImGui::TableNextColumn();
                    ImGui::Text("%s %s", event.direction == "above" ? ">=" : "<=", aegis::FormatCurrency(event.trigger_price).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(event.observed_price).c_str());
                    ImGui::TableNextColumn();
                    const aegis::AlertOutcome outcome = aegis::AlertOutcomeForEvent(event, state_.quotes);
                    ImGui::TextUnformatted(outcome.has_quote ? aegis::FormatCurrency(outcome.current_price).c_str() : "--");
                    ImGui::TableNextColumn();
                    if (outcome.has_quote)
                        TextValueColored(outcome.change, aegis::FormatPercent(outcome.change_percent));
                    else
                        ImGui::TextUnformatted("--");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(event.triggered_at.c_str());
                    ImGui::TableNextColumn();
                    const std::string state = aegis::AlertEventState(event);
                    TextValueColored(state == "Open" ? 1.0 : 0.0, state);
                    ImGui::TableNextColumn();
                    const std::string snooze = aegis::AlertEventSnoozeLabel(event);
                    ImGui::TextUnformatted(snooze.empty() ? "--" : snooze.c_str());
                    ImGui::TableNextColumn();
                    ImGui::PushID(event.id.c_str());
                    if (!event.acknowledged && SmallActionButton("Ack"))
                    {
                        aegis::AcknowledgeAlertEvent(event);
                        aegis::SaveAlertEvents(alert_events_);
                        status_ = event.symbol + " alert acknowledged.";
                    }
                    ImGui::SameLine();
                    if (SmallActionButton("Snooze"))
                    {
                        aegis::SnoozeAlertEvent(event, 30);
                        aegis::SaveAlertEvents(alert_events_);
                        status_ = event.symbol + " alert snoozed for 30 minutes.";
                    }
                    ImGui::PopID();
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", event.note.c_str());
                }
                ImGui::EndTable();
            }
            if (alert_events_.empty())
                TextMuted("No alert triggers recorded yet.");
        }

        void RenderNotificationCenter()
        {
            SectionTitle("Notification Center", alert_monitor_status_.c_str());
            const int unread_alerts = aegis::CountUnreadAlertEvents(alert_events_);
            if (SmallActionButton("Check Now"))
                EvaluateCustomAlerts("notification-center");
            ImGui::SameLine();
            ImGui::TextWrapped("%d unread", unread_alerts);

            if (ImGui::BeginTable("notification_center", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 165.0f)))
            {
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 76.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 88.0f);
                ImGui::TableSetupColumn("Outcome", ImGuiTableColumnFlags_WidthFixed, 118.0f);
                ImGui::TableSetupColumn("Triggered", ImGuiTableColumnFlags_WidthFixed, 118.0f);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Detail");
                ImGui::TableHeadersRow();

                int shown = 0;
                for (int i = static_cast<int>(alert_events_.size()) - 1; i >= 0 && shown < 8; --i)
                {
                    aegis::AlertEvent& event = alert_events_[static_cast<size_t>(i)];
                    if (event.acknowledged)
                        continue;
                    const aegis::AlertOutcome outcome = aegis::AlertOutcomeForEvent(event, state_.quotes);
                    const std::string state = aegis::AlertEventState(event);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(event.symbol.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        selected_symbol_ = event.symbol;
                        selected_tab_ = 4;
                    }
                    ImGui::TableNextColumn();
                    TextValueColored(state == "Open" ? 1.0 : 0.0, state);
                    ImGui::TableNextColumn();
                    if (outcome.has_quote)
                        TextValueColored(outcome.change, (outcome.label + " " + aegis::FormatPercent(outcome.change_percent)));
                    else
                        ImGui::TextUnformatted(outcome.label.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(event.triggered_at.c_str());
                    ImGui::TableNextColumn();
                    ImGui::PushID(event.id.c_str());
                    if (SmallActionButton("Ack"))
                    {
                        aegis::AcknowledgeAlertEvent(event);
                        aegis::SaveAlertEvents(alert_events_);
                        status_ = event.symbol + " alert acknowledged.";
                    }
                    ImGui::SameLine();
                    if (SmallActionButton("Snooze"))
                    {
                        aegis::SnoozeAlertEvent(event, 30);
                        aegis::SaveAlertEvents(alert_events_);
                        status_ = event.symbol + " alert snoozed for 30 minutes.";
                    }
                    ImGui::PopID();
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", outcome.detail.c_str());
                    ++shown;
                }
                if (shown == 0)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Clear");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("0 unread");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("--");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("--");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("--");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("No open alert notifications.");
                }
                ImGui::EndTable();
            }
        }

        void RenderConvictionHeatmap()
        {
            SectionTitle("Heatmaps", "Watchlist movers and conviction grid.");
            const int columns = 4;
            if (ImGui::BeginTable("conviction_heatmap", columns, ImGuiTableFlags_Borders))
            {
                int col = 0;
                for (const aegis::StockSignal& signal : state_.signals)
                {
                    const aegis::StockQuote* quote = FindExactQuote(signal.symbol);
                    ImGui::TableNextColumn();
                    const float score_t = std::clamp(static_cast<float>(signal.score) / 100.0f, 0.0f, 1.0f);
                    const float move = quote ? static_cast<float>(std::clamp((quote->change_percent + 5.0) / 10.0, 0.0, 1.0)) : 0.5f;
                    ImGui::PushStyleColor(ImGuiCol_Button, V4(0.95f - score_t * 0.70f, 0.18f + score_t * 0.58f, 0.20f + move * 0.24f, 0.92f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, V4(0.18f, 0.38f, 0.27f, 1.0f));
                    ImGui::PushID(signal.symbol.c_str());
                    if (ImGui::Button((signal.symbol + "\n" + std::to_string(signal.score)).c_str(), ImVec2(-1.0f, 46.0f)))
                        selected_symbol_ = signal.symbol;
                    ImGui::PopID();
                    ImGui::PopStyleColor(2);
                    ++col;
                    if (col >= columns)
                        col = 0;
                }
                ImGui::EndTable();
            }
        }

        void RenderSymbolNotesPanel()
        {
            SectionTitle("Symbol Notes");
            if (selected_note_index_ < 0 && !selected_symbol_.empty() && note_symbol_buffer_[0] == '\0')
                SetBuffer(note_symbol_buffer_, sizeof(note_symbol_buffer_), selected_symbol_);
            ImGui::InputText("Symbol##note", note_symbol_buffer_, sizeof(note_symbol_buffer_));
            ImGui::InputText("Tags##note", note_tags_buffer_, sizeof(note_tags_buffer_));
            ImGui::InputTextMultiline("Research note##note", note_body_buffer_, sizeof(note_body_buffer_), ImVec2(0, 88.0f));
            if (SmallActionButton(selected_note_index_ >= 0 ? "Update Note" : "Save Note"))
                SaveSymbolNoteFromEditor();
            ImGui::SameLine();
            if (SmallActionButton("Load Selected"))
                LoadNoteForSymbol(selected_symbol_);
            ImGui::SameLine();
            if (SmallActionButton("Clear Note"))
                ClearNoteEditor();
            if (selected_note_index_ >= 0)
            {
                ImGui::SameLine();
                if (SmallActionButton("Remove Note"))
                    RemoveSelectedNote();
            }

            if (ImGui::BeginTable("symbol_notes_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 158.0f)))
            {
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 78.0f);
                ImGui::TableSetupColumn("Updated", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Tags", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Note");
                ImGui::TableHeadersRow();
                for (int i = 0; i < static_cast<int>(symbol_notes_.size()); ++i)
                {
                    const aegis::SymbolNote& note = symbol_notes_[static_cast<size_t>(i)];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(note.symbol.c_str(), selected_note_index_ == i, ImGuiSelectableFlags_SpanAllColumns))
                        LoadNoteEditor(i);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(note.updated_at.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", note.tags.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", note.note.c_str());
                }
                ImGui::EndTable();
            }
        }

        void RenderJournalPanel()
        {
            SectionTitle("Trade Journal Upgrade", "Entry reason, exit reason, tags, mistakes, grade, and win/loss stats.");
            if (journal_symbol_buffer_[0] == '\0' && !selected_symbol_.empty())
                SetBuffer(journal_symbol_buffer_, sizeof(journal_symbol_buffer_), selected_symbol_);
            if (journal_action_buffer_[0] == '\0')
                SetBuffer(journal_action_buffer_, sizeof(journal_action_buffer_), "Review");

            ImGui::SetNextItemWidth(92.0f);
            ImGui::InputText("Symbol##journal", journal_symbol_buffer_, sizeof(journal_symbol_buffer_));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            ImGui::InputText("Action##journal", journal_action_buffer_, sizeof(journal_action_buffer_));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(92.0f);
            ImGui::InputText("Grade##journal", journal_grade_buffer_, sizeof(journal_grade_buffer_));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(116.0f);
            ImGui::InputDouble("Realized P/L", &journal_realized_pnl_, 10.0, 100.0, "%.2f");
            ImGui::InputTextMultiline("Entry reason", journal_reason_buffer_, sizeof(journal_reason_buffer_), ImVec2(0, 58.0f));
            ImGui::InputTextMultiline("Exit reason", journal_exit_buffer_, sizeof(journal_exit_buffer_), ImVec2(0, 48.0f));
            ImGui::InputText("Tags##journal", journal_tags_buffer_, sizeof(journal_tags_buffer_));
            ImGui::InputTextMultiline("Mistakes", journal_mistakes_buffer_, sizeof(journal_mistakes_buffer_), ImVec2(0, 48.0f));
            if (SmallActionButton(selected_journal_index_ >= 0 ? "Update Entry" : "Add Entry"))
                SaveJournalFromEditor();
            ImGui::SameLine();
            if (SmallActionButton("Clear Entry"))
                ClearJournalEditor();
            if (selected_journal_index_ >= 0)
            {
                ImGui::SameLine();
                if (SmallActionButton("Remove Entry"))
                    RemoveSelectedJournalEntry();
            }

            int wins = 0;
            int losses = 0;
            double total_pnl = 0.0;
            for (const aegis::JournalEntry& entry : journal_entries_)
            {
                total_pnl += entry.realized_pnl;
                if (entry.realized_pnl > 0.0)
                    ++wins;
                else if (entry.realized_pnl < 0.0)
                    ++losses;
            }
            ImGui::Text("Entries %d   Wins %d   Losses %d   Win rate %s   Realized %s",
                static_cast<int>(journal_entries_.size()),
                wins,
                losses,
                aegis::FormatPercent((wins + losses) > 0 ? (static_cast<double>(wins) / static_cast<double>(wins + losses)) * 100.0 : 0.0).c_str(),
                aegis::FormatCurrency(total_pnl).c_str());

            if (ImGui::BeginTable("journal_entries_table", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX, ImVec2(0, 190.0f)))
            {
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 76.0f);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableSetupColumn("P/L", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Grade", ImGuiTableColumnFlags_WidthFixed, 66.0f);
                ImGui::TableSetupColumn("Tags", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Reason");
                ImGui::TableSetupColumn("Mistakes");
                ImGui::TableHeadersRow();
                for (int i = 0; i < static_cast<int>(journal_entries_.size()); ++i)
                {
                    const aegis::JournalEntry& entry = journal_entries_[static_cast<size_t>(i)];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.time.c_str());
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(entry.symbol.c_str(), selected_journal_index_ == i, ImGuiSelectableFlags_SpanAllColumns))
                        LoadJournalEditor(i);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.action.c_str());
                    ImGui::TableNextColumn();
                    TextValueColored(entry.realized_pnl, aegis::FormatCurrency(entry.realized_pnl));
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.grade.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", entry.tags.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", entry.reason.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", entry.mistakes.c_str());
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();
            RenderInfoTable("Model Audit", aegis::BuildModelAudit(trade_plans_, state_));
        }

        void RenderRiskUpgradePanel()
        {
            const aegis::RiskSummary risk = aegis::BuildRiskSummary(state_, holdings_, config_.portfolio_cash);
            double marked_value = 0.0;
            double history_weighted_beta = 0.0;
            const aegis::StockQuote* benchmark = FindExactQuote("SPY");
            for (const aegis::PortfolioHolding& holding : holdings_)
            {
                const aegis::StockQuote* quote = FindExactQuote(holding.symbol);
                const double mark = quote != nullptr && quote->price > 0.0 ? quote->price : holding.average_cost;
                const double row_value = holding.shares * mark;
                marked_value += row_value;
                history_weighted_beta += row_value * ReturnBeta(quote, benchmark);
            }
            const double realized_beta = marked_value > 0.0 ? history_weighted_beta / marked_value : risk.beta_exposure;
            SectionTitle("Portfolio Risk Upgrades");
            MetricCard({ "Beta exposure", "", aegis::FormatPercent(realized_beta), "", "Weighted beta from available quote return history vs SPY; falls back to provider beta when history is thin." }, ImVec2(166.0f, 94.0f));
            ImGui::SameLine();
            MetricCard({ "Drawdown est.", "", aegis::FormatPercent(risk.estimated_drawdown), "", "Stress estimate from volatility and concentration." }, ImVec2(166.0f, 94.0f));
            ImGui::SameLine();
            MetricCard({ "Cash drag", "", aegis::FormatPercent(risk.cash_drag), "", "Cash as a share of modeled equity." }, ImVec2(166.0f, 94.0f));
            ImGui::SameLine();
            MetricCard({ "Concentration", "", aegis::FormatPercent(risk.concentration), "", "Largest position share." }, ImVec2(166.0f, 94.0f));
            ImGui::Spacing();
            RenderInfoTable("Risk Detail", risk.rows);

            const PortfolioTotals totals = CalculatePortfolioTotals();
            const std::vector<ExposureRow> sectors = CalculateSectorExposure(totals.market_value);
            ImGui::Spacing();
            SectionTitle("Sector Caps");
            if (ImGui::BeginTable("sector_caps", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Sector");
                ImGui::TableSetupColumn("Alloc", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Cap", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                ImGui::TableSetupColumn("State");
                ImGui::TableHeadersRow();
                for (const ExposureRow& row : sectors)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatPercent(row.percent).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("30.00%");
                    ImGui::TableNextColumn();
                    TextValueColored(row.percent <= 30.0 ? 1.0 : -1.0, row.percent <= 30.0 ? "Inside cap" : "Over cap");
                }
                ImGui::EndTable();
            }
        }

        void RenderCorrelationMatrix()
        {
            SectionTitle("Correlation Matrix", "Calculated from available quote return history; sector fallback is no longer used here.");
            const int limit = std::min(8, static_cast<int>(holdings_.size()));
            if (limit <= 0)
            {
                TextMuted("Add holdings to see estimated cross-position correlation.");
                return;
            }
            if (ImGui::BeginTable("correlation_matrix", limit + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX))
            {
                ImGui::TableSetupColumn("");
                for (int i = 0; i < limit; ++i)
                    ImGui::TableSetupColumn(holdings_[static_cast<size_t>(i)].symbol.c_str(), ImGuiTableColumnFlags_WidthFixed, 62.0f);
                ImGui::TableHeadersRow();
                for (int row = 0; row < limit; ++row)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(holdings_[static_cast<size_t>(row)].symbol.c_str());
                    for (int col = 0; col < limit; ++col)
                    {
                        const aegis::StockQuote* a = FindExactQuote(holdings_[static_cast<size_t>(row)].symbol);
                        const aegis::StockQuote* b = FindExactQuote(holdings_[static_cast<size_t>(col)].symbol);
                        const double corr = row == col ? 1.0 : ReturnCorrelation(a, b);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f", corr);
                    }
                }
                ImGui::EndTable();
            }
        }


        void ClearNoteEditor()
        {
            selected_note_index_ = -1;
            SetBuffer(note_symbol_buffer_, sizeof(note_symbol_buffer_), selected_symbol_);
            SetBuffer(note_tags_buffer_, sizeof(note_tags_buffer_), "");
            SetBuffer(note_body_buffer_, sizeof(note_body_buffer_), "");
        }

        void LoadNoteEditor(int index)
        {
            if (index < 0 || index >= static_cast<int>(symbol_notes_.size()))
                return;
            selected_note_index_ = index;
            const aegis::SymbolNote& note = symbol_notes_[static_cast<size_t>(index)];
            selected_symbol_ = note.symbol;
            SetBuffer(note_symbol_buffer_, sizeof(note_symbol_buffer_), note.symbol);
            SetBuffer(note_tags_buffer_, sizeof(note_tags_buffer_), note.tags);
            SetBuffer(note_body_buffer_, sizeof(note_body_buffer_), note.note);
        }

        void LoadNoteForSymbol(const std::string& symbol)
        {
            const std::string target = aegis::Upper(aegis::Trim(symbol));
            for (int i = 0; i < static_cast<int>(symbol_notes_.size()); ++i)
            {
                if (aegis::Upper(symbol_notes_[static_cast<size_t>(i)].symbol) == target)
                {
                    LoadNoteEditor(i);
                    return;
                }
            }
            selected_note_index_ = -1;
            SetBuffer(note_symbol_buffer_, sizeof(note_symbol_buffer_), target);
            SetBuffer(note_tags_buffer_, sizeof(note_tags_buffer_), "");
            SetBuffer(note_body_buffer_, sizeof(note_body_buffer_), "");
        }

        void SaveSymbolNoteFromEditor()
        {
            aegis::SymbolNote note;
            note.symbol = aegis::Upper(aegis::Trim(note_symbol_buffer_));
            note.tags = aegis::Trim(note_tags_buffer_);
            note.note = aegis::Trim(note_body_buffer_);
            note.updated_at = aegis::NowTimeLabel();
            if (note.symbol.empty())
            {
                status_ = "Enter a symbol for the note.";
                return;
            }

            if (selected_note_index_ >= 0 && selected_note_index_ < static_cast<int>(symbol_notes_.size()))
                symbol_notes_[static_cast<size_t>(selected_note_index_)] = note;
            else
            {
                auto existing = std::find_if(symbol_notes_.begin(), symbol_notes_.end(), [&](const aegis::SymbolNote& row) {
                    return aegis::Upper(row.symbol) == note.symbol;
                });
                if (existing != symbol_notes_.end())
                {
                    *existing = note;
                    selected_note_index_ = static_cast<int>(std::distance(symbol_notes_.begin(), existing));
                }
                else
                {
                    symbol_notes_.push_back(note);
                    selected_note_index_ = static_cast<int>(symbol_notes_.size()) - 1;
                }
            }
            std::sort(symbol_notes_.begin(), symbol_notes_.end(), [](const aegis::SymbolNote& a, const aegis::SymbolNote& b) {
                return a.symbol < b.symbol;
            });
            for (int i = 0; i < static_cast<int>(symbol_notes_.size()); ++i)
            {
                if (symbol_notes_[static_cast<size_t>(i)].symbol == note.symbol)
                    selected_note_index_ = i;
            }
            selected_symbol_ = note.symbol;
            status_ = aegis::SaveSymbolNotes(symbol_notes_) ? "Symbol note saved." : "Could not save symbol note.";
            Audit("note_saved", note.symbol, note.tags);
        }

        void RemoveSelectedNote()
        {
            if (selected_note_index_ < 0 || selected_note_index_ >= static_cast<int>(symbol_notes_.size()))
                return;
            const std::string symbol = symbol_notes_[static_cast<size_t>(selected_note_index_)].symbol;
            symbol_notes_.erase(symbol_notes_.begin() + selected_note_index_);
            selected_note_index_ = -1;
            ClearNoteEditor();
            status_ = aegis::SaveSymbolNotes(symbol_notes_) ? "Symbol note removed." : "Could not save symbol notes.";
            Audit("note_removed", symbol, "");
        }

        void ClearJournalEditor()
        {
            selected_journal_index_ = -1;
            SetBuffer(journal_symbol_buffer_, sizeof(journal_symbol_buffer_), selected_symbol_);
            SetBuffer(journal_action_buffer_, sizeof(journal_action_buffer_), "Review");
            SetBuffer(journal_reason_buffer_, sizeof(journal_reason_buffer_), "");
            SetBuffer(journal_exit_buffer_, sizeof(journal_exit_buffer_), "");
            SetBuffer(journal_tags_buffer_, sizeof(journal_tags_buffer_), "");
            SetBuffer(journal_mistakes_buffer_, sizeof(journal_mistakes_buffer_), "");
            SetBuffer(journal_grade_buffer_, sizeof(journal_grade_buffer_), "");
            journal_realized_pnl_ = 0.0;
        }

        void LoadJournalEditor(int index)
        {
            if (index < 0 || index >= static_cast<int>(journal_entries_.size()))
                return;
            selected_journal_index_ = index;
            const aegis::JournalEntry& entry = journal_entries_[static_cast<size_t>(index)];
            selected_symbol_ = entry.symbol;
            SetBuffer(journal_symbol_buffer_, sizeof(journal_symbol_buffer_), entry.symbol);
            SetBuffer(journal_action_buffer_, sizeof(journal_action_buffer_), entry.action);
            SetBuffer(journal_reason_buffer_, sizeof(journal_reason_buffer_), entry.reason);
            SetBuffer(journal_exit_buffer_, sizeof(journal_exit_buffer_), entry.exit_reason);
            SetBuffer(journal_tags_buffer_, sizeof(journal_tags_buffer_), entry.tags);
            SetBuffer(journal_mistakes_buffer_, sizeof(journal_mistakes_buffer_), entry.mistakes);
            SetBuffer(journal_grade_buffer_, sizeof(journal_grade_buffer_), entry.grade);
            journal_realized_pnl_ = entry.realized_pnl;
        }

        void SaveJournalFromEditor()
        {
            const std::string existing_time = selected_journal_index_ >= 0 && selected_journal_index_ < static_cast<int>(journal_entries_.size())
                ? journal_entries_[static_cast<size_t>(selected_journal_index_)].time
                : aegis::NowTimeLabel();
            const aegis::JournalDraftResult draft = aegis::ValidateJournalDraft(existing_time,
                journal_symbol_buffer_,
                journal_action_buffer_,
                journal_reason_buffer_,
                journal_exit_buffer_,
                journal_tags_buffer_,
                journal_mistakes_buffer_,
                journal_grade_buffer_,
                journal_realized_pnl_);
            if (!draft.ok)
            {
                status_ = "Journal rejected: " + draft.error;
                return;
            }
            const aegis::JournalEntry entry = draft.row;

            if (selected_journal_index_ >= 0 && selected_journal_index_ < static_cast<int>(journal_entries_.size()))
                journal_entries_[static_cast<size_t>(selected_journal_index_)] = entry;
            else
            {
                journal_entries_.push_back(entry);
                selected_journal_index_ = static_cast<int>(journal_entries_.size()) - 1;
            }
            selected_symbol_ = entry.symbol;
            status_ = aegis::SaveJournalEntries(journal_entries_) ? "Journal entry saved." : "Could not save journal entries.";
            Audit("journal_saved", entry.symbol, entry.action + " " + entry.grade);
        }

        void RemoveSelectedJournalEntry()
        {
            if (selected_journal_index_ < 0 || selected_journal_index_ >= static_cast<int>(journal_entries_.size()))
                return;
            const std::string symbol = journal_entries_[static_cast<size_t>(selected_journal_index_)].symbol;
            journal_entries_.erase(journal_entries_.begin() + selected_journal_index_);
            selected_journal_index_ = -1;
            ClearJournalEditor();
            status_ = aegis::SaveJournalEntries(journal_entries_) ? "Journal entry removed." : "Could not save journal entries.";
            Audit("journal_removed", symbol, "");
        }

        void Audit(const std::string& action, const std::string& symbol, const std::string& detail)
        {
            aegis::AppendAuditEvent({ username_.empty() ? "local" : username_, action, symbol, detail });
        }

        std::string DefaultCompareSymbols() const
        {
            std::vector<std::string> symbols = aegis::SplitWatchlist(config_.watchlist);
            if (symbols.empty())
                symbols = { "AAPL", "MSFT", "NVDA", "SPY" };
            if (symbols.size() > 4)
                symbols.resize(4);
            return aegis::JoinWatchlist(symbols);
        }

        void LoadSessionState()
        {
            const aegis::AppSessionState session = aegis::LoadAppSessionState();
            if (!session.loaded)
                return;

            selected_tab_ = session.selected_tab;
            if (!session.selected_symbol.empty())
                selected_symbol_ = session.selected_symbol;
            if (!session.compare_symbols.empty())
                SetBuffer(compare_symbols_buffer_, sizeof(compare_symbols_buffer_), session.compare_symbols);
            chart_days_ = session.chart_days;
            chart_aggregation_ = session.chart_aggregation;
            show_chart_volume_ = session.show_chart_volume;
            show_trend_indicators_ = session.show_trend_indicators;
            show_momentum_indicators_ = session.show_momentum_indicators;
            show_volatility_indicators_ = session.show_volatility_indicators;
            show_relative_indicators_ = session.show_relative_indicators;
            if (!session.strategy_rule.empty())
                SetBuffer(strategy_rule_buffer_, sizeof(strategy_rule_buffer_), session.strategy_rule);
        }

        void SaveSessionState() const
        {
            aegis::AppSessionState session;
            session.selected_tab = selected_tab_;
            session.selected_symbol = selected_symbol_;
            session.compare_symbols = compare_symbols_buffer_;
            session.chart_days = chart_days_;
            session.chart_aggregation = chart_aggregation_;
            session.show_chart_volume = show_chart_volume_;
            session.show_trend_indicators = show_trend_indicators_;
            session.show_momentum_indicators = show_momentum_indicators_;
            session.show_volatility_indicators = show_volatility_indicators_;
            session.show_relative_indicators = show_relative_indicators_;
            session.strategy_rule = strategy_rule_buffer_;
            aegis::SaveAppSessionState(session);
        }

        aegis::PersistentAppData CurrentPersistentData() const
        {
            aegis::PersistentAppData data;
            data.holdings = holdings_;
            data.price_alerts = price_alerts_;
            data.alert_events = alert_events_;
            data.trade_plans = trade_plans_;
            data.symbol_notes = symbol_notes_;
            data.journal_entries = journal_entries_;
            return data;
        }

        std::vector<aegis::InfoItem> BuildImportPreviewRows() const
        {
            const std::filesystem::path dir = aegis::AppDataDirectory();
            const auto item_from_counts = [](const char* file_name, bool exists, int valid, int errors, const std::string& first_error, const char* detail) {
                aegis::InfoItem item;
                item.name = file_name;
                item.state = exists ? (errors == 0 ? "Ready" : "Review") : "Missing";
                item.value = exists ? (std::to_string(valid) + " valid, " + std::to_string(errors) + " rejected") : "--";
                if (!exists)
                {
                    item.detail = "Place this file in the app data folder to import it.";
                    return item;
                }
                item.detail = detail;
                if (!first_error.empty())
                    item.detail += " First issue: " + first_error;
                return item;
            };

            const auto preview_watchlist = [&]() {
                const char* file_name = "watchlist-import.csv";
                std::ifstream file(dir / file_name);
                if (!file)
                    return item_from_counts(file_name, false, 0, 0, {}, "Ticker symbols, one per cell or comma-separated.");

                int valid = 0;
                int errors = 0;
                std::string first_error;
                std::string line;
                while (std::getline(file, line))
                {
                    if (aegis::Trim(line).empty())
                        continue;
                    for (const std::string& cell : aegis::SplitCsvLine(line))
                    {
                        const std::string token = aegis::Trim(cell);
                        if (token.empty() || aegis::Lower(token) == "symbol")
                            continue;
                        const aegis::SymbolValidationResult symbol = aegis::ValidateTickerSymbol(token);
                        if (symbol.ok)
                            ++valid;
                        else
                        {
                            ++errors;
                            if (first_error.empty())
                                first_error = token + ": " + symbol.reason;
                        }
                    }
                }
                return item_from_counts(file_name, true, valid, errors, first_error, "Ticker symbols, one per cell or comma-separated.");
            };

            const auto preview = [&](const char* file_name, const char* detail, auto validate) {
                std::ifstream file(dir / file_name);
                if (!file)
                    return item_from_counts(file_name, false, 0, 0, {}, detail);

                int valid = 0;
                int errors = 0;
                int row_number = 1;
                bool header = true;
                std::string first_error;
                std::string line;
                while (std::getline(file, line))
                {
                    if (header)
                    {
                        header = false;
                        continue;
                    }
                    ++row_number;
                    if (aegis::Trim(line).empty())
                        continue;
                    const std::vector<std::string> cells = aegis::SplitCsvLine(line);
                    const std::string error = validate(cells);
                    if (error.empty())
                        ++valid;
                    else
                    {
                        ++errors;
                        if (first_error.empty())
                            first_error = "row " + std::to_string(row_number) + ": " + error;
                    }
                }
                return item_from_counts(file_name, true, valid, errors, first_error, detail);
            };

            return {
                preview_watchlist(),
                preview("holdings-import.csv", "symbol, shares, average_cost, optional note.", [](const std::vector<std::string>& cells) { return aegis::ParseHoldingImportRow(cells).error; }),
                preview("alerts-import.csv", "symbol, trigger_price, above/below, optional enabled and note.", [](const std::vector<std::string>& cells) { return aegis::ParseAlertImportRow(cells).error; }),
                preview("trade-plans-import.csv", "created_at, symbol, rating, entry, stop, target, shares, risk, reward, status, thesis.", [](const std::vector<std::string>& cells) { return aegis::ParseTradePlanImportRow(cells).error; }),
                preview("symbol-notes-import.csv", "symbol, tags, note, optional updated_at.", [](const std::vector<std::string>& cells) { return aegis::ParseSymbolNoteImportRow(cells).error; }),
                preview("trade-journal-import.csv", "time, symbol, action, reason, exit_reason, tags, mistakes, grade, optional realized_pnl.", [](const std::vector<std::string>& cells) { return aegis::ParseJournalImportRow(cells).error; })
            };
        }

        void RefreshBrokerPreview()
        {
            broker_preview_ = aegis::PreviewBrokerImport(aegis::AppDataDirectory());
            const int valid = aegis::CountValidBrokerRows(broker_preview_);
            broker_import_status_ = broker_preview_.empty()
                ? "No broker import files found."
                : "Broker preview loaded: " + std::to_string(valid) + " valid row(s), " + std::to_string(static_cast<int>(broker_preview_.size()) - valid) + " row(s) need review.";
        }

        void ImportBrokerPreviewRows()
        {
            if (broker_preview_.empty())
                RefreshBrokerPreview();

            int imported = 0;
            int updated = 0;
            for (const aegis::BrokerImportRow& row : broker_preview_)
            {
                if (!row.valid)
                    continue;
                auto existing = std::find_if(holdings_.begin(), holdings_.end(), [&](const aegis::PortfolioHolding& holding) {
                    return aegis::Upper(holding.symbol) == row.symbol;
                });
                if (existing == holdings_.end())
                {
                    aegis::PortfolioHolding holding;
                    holding.symbol = row.symbol;
                    holding.shares = row.shares;
                    holding.average_cost = row.average_cost;
                    holding.note = row.note.empty() ? "Broker import" : row.note;
                    holdings_.push_back(holding);
                    ++imported;
                }
                else
                {
                    const double old_value = existing->shares * existing->average_cost;
                    const double new_value = row.shares * row.average_cost;
                    const double combined_shares = existing->shares + row.shares;
                    existing->average_cost = combined_shares > 0.0 ? (old_value + new_value) / combined_shares : existing->average_cost;
                    existing->shares = combined_shares;
                    if (!row.note.empty())
                    {
                        if (!existing->note.empty())
                            existing->note += "; ";
                        existing->note += row.note;
                    }
                    ++updated;
                }
            }

            std::sort(holdings_.begin(), holdings_.end(), [](const aegis::PortfolioHolding& a, const aegis::PortfolioHolding& b) {
                return a.symbol < b.symbol;
            });
            EnsureHoldingsInWatchlist();
            SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
            aegis::SavePortfolioHoldings(holdings_);
            aegis::SaveConfig(config_);
            broker_import_status_ = "Broker import complete: " + std::to_string(imported) + " new holding(s), " + std::to_string(updated) + " updated holding(s).";
            status_ = broker_import_status_;
            Audit("broker_import", "", broker_import_status_);
            BeginRefresh(false);
        }

        void RenderBrokerImportPanel()
        {
            SectionTitle("Broker Import Normalizer", "Preview Robinhood, Fidelity, Schwab, Webull, IBKR, or generic broker CSVs before importing holdings.");
            ImGui::TextWrapped("%s", aegis::BrokerImportInstructions().c_str());
            if (SmallActionButton("Refresh Broker Preview"))
                RefreshBrokerPreview();
            ImGui::SameLine();
            if (SmallActionButton("Import Valid Broker Rows"))
                ImportBrokerPreviewRows();
            ImGui::TextWrapped("%s", broker_import_status_.c_str());

            if (broker_preview_.empty())
            {
                TextMuted("No broker rows loaded.");
                return;
            }

            if (ImGui::BeginTable("broker_import_preview", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX, ImVec2(0, 210.0f)))
            {
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 74.0f);
                ImGui::TableSetupColumn("Broker", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthFixed, 138.0f);
                ImGui::TableSetupColumn("Row", ImGuiTableColumnFlags_WidthFixed, 48.0f);
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 76.0f);
                ImGui::TableSetupColumn("Shares", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableSetupColumn("Avg Cost", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                ImGui::TableSetupColumn("Message");
                ImGui::TableHeadersRow();
                for (const aegis::BrokerImportRow& row : broker_preview_)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    TextValueColored(row.valid ? 1.0 : -1.0, row.status);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row.profile.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row.source_file.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", row.row_number);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row.symbol.empty() ? "--" : row.symbol.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.4g", row.shares);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row.average_cost > 0.0 ? aegis::FormatCurrency(row.average_cost).c_str() : "--");
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", row.valid ? row.note.c_str() : row.error.c_str());
                }
                ImGui::EndTable();
            }
        }

        void ExportWorkflowCsv()
        {
            const std::filesystem::path dir = aegis::AppDataDirectory();
            {
                std::ofstream file(dir / "holdings-export.csv");
                file << "symbol,shares,average_cost,note\n";
                for (const aegis::PortfolioHolding& row : holdings_)
                    file << aegis::CsvCell(row.symbol) << ',' << row.shares << ',' << row.average_cost << ',' << aegis::CsvCell(row.note) << '\n';
            }
            {
                std::ofstream file(dir / "alerts-export.csv");
                file << "symbol,trigger_price,direction,enabled,note\n";
                for (const aegis::PriceAlert& row : price_alerts_)
                    file << aegis::CsvCell(row.symbol) << ',' << row.trigger_price << ',' << aegis::CsvCell(row.above ? "above" : "below") << ',' << (row.enabled ? "true" : "false") << ',' << aegis::CsvCell(row.note) << '\n';
            }
            {
                std::ofstream file(dir / "alert-events-export.csv");
                file << "id,alert_key,symbol,direction,trigger_price,observed_price,triggered_at,source,note,acknowledged,acknowledged_at,snoozed_until_epoch\n";
                for (const aegis::AlertEvent& row : alert_events_)
                    file << aegis::CsvCell(row.id) << ',' << aegis::CsvCell(row.alert_key) << ',' << aegis::CsvCell(row.symbol) << ',' << aegis::CsvCell(row.direction) << ',' << row.trigger_price << ',' << row.observed_price << ',' << aegis::CsvCell(row.triggered_at) << ',' << aegis::CsvCell(row.source) << ',' << aegis::CsvCell(row.note) << ',' << (row.acknowledged ? "true" : "false") << ',' << aegis::CsvCell(row.acknowledged_at) << ',' << row.snoozed_until_epoch << '\n';
            }
            {
                std::ofstream file(dir / "trade-plans-export.csv");
                file << "created_at,symbol,rating,entry,stop,target,shares,planned_risk,planned_reward,status,thesis\n";
                for (const aegis::TradePlan& row : trade_plans_)
                    file << aegis::CsvCell(row.created_at) << ',' << aegis::CsvCell(row.symbol) << ',' << aegis::CsvCell(row.rating) << ',' << row.entry << ',' << row.stop << ',' << row.target << ',' << row.shares << ',' << row.planned_risk << ',' << row.planned_reward << ',' << aegis::CsvCell(row.status) << ',' << aegis::CsvCell(row.thesis) << '\n';
            }
            {
                std::ofstream file(dir / "symbol-notes-export.csv");
                file << "symbol,tags,note,updated_at\n";
                for (const aegis::SymbolNote& row : symbol_notes_)
                    file << aegis::CsvCell(row.symbol) << ',' << aegis::CsvCell(row.tags) << ',' << aegis::CsvCell(row.note) << ',' << aegis::CsvCell(row.updated_at) << '\n';
            }
            {
                std::ofstream file(dir / "trade-journal-export.csv");
                file << "time,symbol,action,reason,exit_reason,tags,mistakes,grade,realized_pnl\n";
                for (const aegis::JournalEntry& row : journal_entries_)
                    file << aegis::CsvCell(row.time) << ',' << aegis::CsvCell(row.symbol) << ',' << aegis::CsvCell(row.action) << ',' << aegis::CsvCell(row.reason) << ',' << aegis::CsvCell(row.exit_reason) << ',' << aegis::CsvCell(row.tags) << ',' << aegis::CsvCell(row.mistakes) << ',' << aegis::CsvCell(row.grade) << ',' << row.realized_pnl << '\n';
            }
            status_ = "CSV pack exported.";
            Audit("export_csv_pack", "", dir.string());
        }

        void ImportWorkflowCsv()
        {
            const std::filesystem::path dir = aegis::AppDataDirectory();
            int imported = 0;
            int rejected = 0;
            import_issues_.clear();
            const auto record_rejection = [&](const char* file_name, int row, const std::string& value, const std::string& reason) {
                ++rejected;
                if (import_issues_.size() < 25)
                    import_issues_.push_back({ file_name, row, value, reason });
            };
            {
                const char* file_name = "watchlist-import.csv";
                std::ifstream file(dir / file_name);
                if (file)
                {
                    std::vector<std::string> symbols = aegis::SplitWatchlist(config_.watchlist);
                    std::string line;
                    int row_number = 0;
                    while (std::getline(file, line))
                    {
                        ++row_number;
                        for (std::string cell : aegis::SplitCsvLine(line))
                        {
                            cell = aegis::Trim(cell);
                            if (cell.empty() || aegis::Lower(cell) == "symbol")
                                continue;
                            const aegis::SymbolValidationResult symbol = aegis::ValidateTickerSymbol(cell);
                            if (!symbol.ok)
                            {
                                record_rejection(file_name, row_number, cell, symbol.reason);
                                continue;
                            }
                            if (std::find(symbols.begin(), symbols.end(), symbol.symbol) == symbols.end())
                            {
                                symbols.push_back(symbol.symbol);
                                ++imported;
                            }
                        }
                    }
                    config_.watchlist = aegis::JoinWatchlist(symbols);
                    SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
                }
            }
            {
                const char* file_name = "holdings-import.csv";
                std::ifstream file(dir / file_name);
                std::string line;
                bool header = true;
                int row_number = 0;
                while (std::getline(file, line))
                {
                    ++row_number;
                    if (header)
                    {
                        header = false;
                        continue;
                    }
                    const std::vector<std::string> cells = aegis::SplitCsvLine(line);
                    const aegis::HoldingImportResult parsed = aegis::ParseHoldingImportRow(cells);
                    if (parsed.ok)
                    {
                        holdings_.push_back(parsed.row);
                        ++imported;
                    }
                    else if (!aegis::Trim(line).empty())
                    {
                        const std::string value = cells.empty() ? aegis::Trim(line) : aegis::Trim(cells.front());
                        record_rejection(file_name, row_number, value, parsed.error);
                    }
                }
            }
            {
                const char* file_name = "alerts-import.csv";
                std::ifstream file(dir / file_name);
                std::string line;
                bool header = true;
                int row_number = 0;
                while (std::getline(file, line))
                {
                    ++row_number;
                    if (header)
                    {
                        header = false;
                        continue;
                    }
                    const std::vector<std::string> cells = aegis::SplitCsvLine(line);
                    const aegis::AlertImportResult parsed = aegis::ParseAlertImportRow(cells);
                    if (parsed.ok)
                    {
                        price_alerts_.push_back(parsed.row);
                        ++imported;
                    }
                    else if (!aegis::Trim(line).empty())
                    {
                        const std::string value = cells.empty() ? aegis::Trim(line) : aegis::Trim(cells.front());
                        record_rejection(file_name, row_number, value, parsed.error);
                    }
                }
            }
            {
                const char* file_name = "trade-plans-import.csv";
                std::ifstream file(dir / file_name);
                std::string line;
                bool header = true;
                int row_number = 0;
                while (std::getline(file, line))
                {
                    ++row_number;
                    if (header)
                    {
                        header = false;
                        continue;
                    }
                    const std::vector<std::string> cells = aegis::SplitCsvLine(line);
                    const aegis::TradePlanImportResult parsed = aegis::ParseTradePlanImportRow(cells);
                    if (parsed.ok)
                    {
                        trade_plans_.push_back(parsed.row);
                        ++imported;
                    }
                    else if (!aegis::Trim(line).empty())
                    {
                        const std::string value = cells.size() > 1 ? aegis::Trim(cells[1]) : aegis::Trim(line);
                        record_rejection(file_name, row_number, value, parsed.error);
                    }
                }
            }
            {
                const char* file_name = "symbol-notes-import.csv";
                std::ifstream file(dir / file_name);
                std::string line;
                bool header = true;
                int row_number = 0;
                while (std::getline(file, line))
                {
                    ++row_number;
                    if (header)
                    {
                        header = false;
                        continue;
                    }
                    const std::vector<std::string> cells = aegis::SplitCsvLine(line);
                    const aegis::SymbolNoteImportResult parsed = aegis::ParseSymbolNoteImportRow(cells);
                    if (parsed.ok)
                    {
                        symbol_notes_.push_back(parsed.row);
                        ++imported;
                    }
                    else if (!aegis::Trim(line).empty())
                    {
                        const std::string value = cells.empty() ? aegis::Trim(line) : aegis::Trim(cells.front());
                        record_rejection(file_name, row_number, value, parsed.error);
                    }
                }
            }
            {
                const char* file_name = "trade-journal-import.csv";
                std::ifstream file(dir / file_name);
                std::string line;
                bool header = true;
                int row_number = 0;
                while (std::getline(file, line))
                {
                    ++row_number;
                    if (header)
                    {
                        header = false;
                        continue;
                    }
                    const std::vector<std::string> cells = aegis::SplitCsvLine(line);
                    const aegis::JournalImportResult parsed = aegis::ParseJournalImportRow(cells);
                    if (parsed.ok)
                    {
                        journal_entries_.push_back(parsed.row);
                        ++imported;
                    }
                    else if (!aegis::Trim(line).empty())
                    {
                        const std::string value = cells.size() > 1 ? aegis::Trim(cells[1]) : aegis::Trim(line);
                        record_rejection(file_name, row_number, value, parsed.error);
                    }
                }
            }

            EnsureHoldingsInWatchlist();
            aegis::SavePersistentAppData(CurrentPersistentData());
            aegis::SaveConfig(config_);
            if (imported > 0 || rejected > 0)
            {
                status_ = "CSV import complete: " + std::to_string(imported) + " imported, " + std::to_string(rejected) + " rejected.";
                if (!import_issues_.empty())
                {
                    const ImportIssue& first = import_issues_.front();
                    status_ += " First issue: " + first.file + " row " + std::to_string(first.row) + " - " + first.reason;
                }
            }
            else
                status_ = "No import CSV files found.";
            Audit("import_csv_pack", "", std::to_string(imported) + " imported, " + std::to_string(rejected) + " rejected");
            BeginRefresh(false);
        }

        void ExportSelectedReport()
        {
            const aegis::StockQuote* quote = FindExactQuote(selected_symbol_);
            const aegis::StockSignal* signal = FindExactSignal(selected_symbol_);
            if (quote == nullptr || signal == nullptr)
            {
                status_ = "Select a loaded symbol before exporting a research report.";
                return;
            }

            std::vector<aegis::Candle> candles = history_symbol_ == quote->symbol && !history_result_.candles.empty()
                ? history_result_.candles
                : aegis::BuildSyntheticCandles(*quote, 365);
            const aegis::IndicatorSnapshot indicators = aegis::BuildIndicators(candles, FindExactQuote("SPY"), FindExactQuote("QQQ"));
            const std::vector<aegis::ScreenPreset> presets = aegis::BuildScreenPresets();
            const int preset_index = std::clamp(chart_preset_, 0, std::max(0, static_cast<int>(presets.size()) - 1));
            const aegis::BacktestResult backtest = aegis::RunSignalBacktest(candles, presets.empty() ? "Momentum" : presets[static_cast<size_t>(preset_index)].name);
            const aegis::StrategyBacktestResult strategy_backtest = aegis::RunStrategyRuleBacktest(candles, strategy_rule_buffer_, strategy_fee_bps_, strategy_slippage_bps_);
            const bool has_research = research_symbol_ == quote->symbol && research_result_.ok;
            const aegis::FundamentalSnapshot fundamentals = has_research && !research_result_.fundamentals.symbol.empty()
                ? research_result_.fundamentals
                : aegis::BuildFundamentals(*quote);
            const std::vector<aegis::FilingItem> filings = has_research && !research_result_.filings.empty()
                ? research_result_.filings
                : aegis::BuildFilings(quote->symbol);
            const std::vector<aegis::NewsItem> news = has_research && !research_result_.news.empty()
                ? research_result_.news
                : aegis::BuildNews(quote->symbol);
            const std::vector<aegis::EarningsItem> earnings = has_research && !research_result_.earnings.empty()
                ? research_result_.earnings
                : aegis::BuildEarnings(quote->symbol);
            ExportResearchReport(*quote, *signal, indicators, backtest, strategy_backtest, fundamentals, filings, news, earnings);
        }

        void ExportResearchReport(const aegis::StockQuote& quote,
            const aegis::StockSignal& signal,
            const aegis::IndicatorSnapshot& indicators,
            const aegis::BacktestResult& backtest,
            const aegis::StrategyBacktestResult& strategy_backtest,
            const aegis::FundamentalSnapshot& fundamentals,
            const std::vector<aegis::FilingItem>& filings,
            const std::vector<aegis::NewsItem>& news,
            const std::vector<aegis::EarningsItem>& earnings)
        {
            aegis::SymbolResearchReport report;
            report.quote = quote;
            report.signal = signal;
            report.indicators = indicators;
            report.backtest = backtest;
            report.strategy_backtest = strategy_backtest.backtest;
            report.strategy_rule = aegis::Trim(strategy_rule_buffer_);
            report.strategy_status = strategy_backtest.status;
            report.fundamentals = fundamentals;
            report.filings = filings;
            report.news = news;
            report.earnings = earnings;
            report.checklist = aegis::BuildTradeChecklist(quote, signal, indicators);
            report.explanation = aegis::BuildModelExplanation(quote, signal, indicators);
            report.risk_critic = aegis::BuildRiskCritic(quote, signal, indicators);
            report.history_source = history_symbol_ == quote.symbol && !history_result_.source.empty() ? history_result_.source : "Generated chart fallback";
            report.history_freshness = history_symbol_ == quote.symbol && !history_result_.cache_age_label.empty() ? history_result_.cache_age_label : history_status_;
            report.history_fallback = history_symbol_ == quote.symbol ? history_result_.fallback_reason : "";
            report.research_source = research_symbol_ == quote.symbol && !research_result_.source.empty() ? research_result_.source : "Generated research fallback";
            report.research_freshness = research_symbol_ == quote.symbol && !research_result_.cache_age_label.empty() ? research_result_.cache_age_label : research_status_;
            report.research_fallback = research_symbol_ == quote.symbol ? research_result_.fallback_reason : "";

            std::filesystem::path output;
            if (aegis::ExportSymbolResearchReport(report, output, report_status_))
            {
                status_ = quote.symbol + " research report exported.";
                Audit("export_symbol_report", quote.symbol, output.string());
                aegis::OpenExternalUrl(output.string());
            }
            else
            {
                status_ = report_status_;
            }
        }

        void ExportDailyBriefing()
        {
            aegis::DailyBriefingReport report;
            report.state = state_;
            report.focus = aegis::BuildFocusItems(state_, price_alerts_);
            report.alerts = price_alerts_;
            report.holdings = holdings_;
            report.alert_events = alert_events_;
            report.cash = config_.portfolio_cash;

            std::filesystem::path output;
            if (aegis::ExportDailyBriefingReport(report, output, report_status_))
            {
                status_ = "Today's briefing exported.";
                Audit("export_daily_briefing", "", output.string());
                aegis::OpenExternalUrl(output.string());
            }
            else
            {
                status_ = report_status_;
            }
        }

        void ExportAllData()
        {
            ExportWorkflowCsv();
            ExportJournal();
            std::ofstream file(aegis::AppDataDirectory() / "aegis-backup-manifest.txt");
            file << "Aegis Stock Investing AI backup\n";
            file << "Created: " << aegis::NowTimeLabel() << '\n';
            file << "Config, encrypted credentials, TSV stores, CSV exports, diagnostics, and journal files live in this folder.\n";
            status_ = "All local data export refreshed.";
            Audit("backup_all_data", "", aegis::AppDataDirectory().string());
        }

        void RunCommandPalette()
        {
            std::string command = aegis::Trim(command_buffer_);
            SetBuffer(command_buffer_, sizeof(command_buffer_), "");
            if (command.empty())
                return;

            std::string lower = command;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            const auto select_symbol = [&](const std::string& raw_symbol, bool add_to_watchlist) {
                const aegis::SymbolValidationResult symbol = aegis::ValidateTickerSymbol(raw_symbol);
                if (!symbol.ok)
                {
                    status_ = "Ticker rejected: " + symbol.reason;
                    return false;
                }
                selected_symbol_ = symbol.symbol;
                SetBuffer(symbol_buffer_, sizeof(symbol_buffer_), symbol.symbol);
                if (add_to_watchlist)
                {
                    std::vector<std::string> symbols = aegis::SplitWatchlist(config_.watchlist);
                    if (std::find(symbols.begin(), symbols.end(), symbol.symbol) == symbols.end())
                    {
                        symbols.push_back(symbol.symbol);
                        config_.watchlist = aegis::JoinWatchlist(symbols);
                        SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
                        aegis::SaveConfig(config_);
                        BeginRefresh(false);
                    }
                }
                selected_tab_ = 4;
                status_ = symbol.symbol + " selected from command palette.";
                return true;
            };
            if (lower == "refresh")
            {
                BeginRefresh(false);
                return;
            }
            if (lower == "settings" || lower == "open settings")
            {
                selected_tab_ = 9;
                return;
            }
            if (lower == "alerts" || lower == "notifications" || lower == "open alerts")
            {
                selected_tab_ = 7;
                return;
            }
            if (lower == "web" || lower == "open web")
            {
                aegis::OpenExternalUrl(aegis::JoinUrl(config_.auth_base_url, config_.website_path));
                return;
            }
            if (lower == "briefing" || lower == "export briefing")
            {
                ExportDailyBriefing();
                return;
            }
            if (lower == "report" || lower == "export report")
            {
                ExportSelectedReport();
                return;
            }
            if (lower.rfind("add ", 0) == 0)
            {
                SetBuffer(symbol_buffer_, sizeof(symbol_buffer_), command.substr(4));
                AddSymbolFromBuffer();
                return;
            }
            if (lower.rfind("search ", 0) == 0)
            {
                select_symbol(command.substr(7), true);
                return;
            }
            if (lower.rfind("open ", 0) == 0)
            {
                select_symbol(command.substr(5), true);
                return;
            }
            if (lower.rfind("ticker ", 0) == 0)
            {
                select_symbol(command.substr(7), true);
                return;
            }
            if (lower.rfind("alert ", 0) == 0)
            {
                std::stringstream stream(command.substr(6));
                std::vector<std::string> tokens;
                std::string token;
                while (stream >> token)
                    tokens.push_back(token);
                std::string symbol = selected_symbol_;
                bool has_direction = false;
                bool has_price = false;
                bool above = true;
                double price = 0.0;
                for (const std::string& raw : tokens)
                {
                    const std::string t = aegis::Lower(raw);
                    if (t == "above" || t == "over" || t == ">")
                    {
                        above = true;
                        has_direction = true;
                        continue;
                    }
                    if (t == "below" || t == "under" || t == "<")
                    {
                        above = false;
                        has_direction = true;
                        continue;
                    }
                    char* end = nullptr;
                    const double parsed = std::strtod(raw.c_str(), &end);
                    if (end != raw.c_str() && end != nullptr && *end == '\0')
                    {
                        price = parsed;
                        has_price = price > 0.0;
                        continue;
                    }
                    const aegis::SymbolValidationResult parsed_symbol = aegis::ValidateTickerSymbol(raw);
                    if (parsed_symbol.ok)
                        symbol = parsed_symbol.symbol;
                }
                if (!has_direction || !has_price)
                {
                    status_ = "Alert command needs a direction and price, like: alert NVDA above 900.";
                    return;
                }
                selected_alert_index_ = -1;
                alert_above_ = above;
                alert_enabled_ = true;
                alert_trigger_price_ = price;
                SetBuffer(alert_symbol_buffer_, sizeof(alert_symbol_buffer_), symbol);
                SetBuffer(alert_note_buffer_, sizeof(alert_note_buffer_), "Created from command palette.");
                selected_symbol_ = symbol;
                selected_tab_ = 4;
                SaveAlertFromEditor();
                return;
            }
            if (lower.rfind("tab ", 0) == 0)
            {
                const std::string tab = lower.substr(4);
                if (tab.find("dash") != std::string::npos) selected_tab_ = 0;
                else if (tab.find("watch") != std::string::npos) selected_tab_ = 1;
                else if (tab.find("scan") != std::string::npos) selected_tab_ = 2;
                else if (tab.find("port") != std::string::npos) selected_tab_ = 3;
                else if (tab.find("research") != std::string::npos) selected_tab_ = 4;
                else if (tab.find("chart") != std::string::npos) selected_tab_ = 5;
                else if (tab.find("compare") != std::string::npos) selected_tab_ = 6;
                else if (tab.find("journal") != std::string::npos) selected_tab_ = 7;
                else if (tab.find("integration") != std::string::npos) selected_tab_ = 8;
                else if (tab.find("settings") != std::string::npos) selected_tab_ = 9;
                return;
            }
            if (lower.rfind("note ", 0) == 0)
            {
                SetBuffer(note_symbol_buffer_, sizeof(note_symbol_buffer_), selected_symbol_);
                SetBuffer(note_body_buffer_, sizeof(note_body_buffer_), command.substr(5));
                SetBuffer(note_tags_buffer_, sizeof(note_tags_buffer_), "command");
                SaveSymbolNoteFromEditor();
                selected_tab_ = 7;
                return;
            }
            if (lower == "plan")
            {
                selected_tab_ = 3;
                SaveTradePlanFromWorksheet();
                return;
            }
            if (select_symbol(command, true))
                return;
            status_ = "Unknown command. Try refresh, add TICKER, search TICKER, alert TICKER above PRICE, report, briefing, tab NAME, note TEXT, or plan.";
        }

        void HandleKeyboardShortcuts(ImGuiIO& io)
        {
            if (io.WantTextInput || !io.KeyCtrl)
                return;
            if (ImGui::IsKeyPressed(ImGuiKey_R))
                BeginRefresh(false);
            if (ImGui::IsKeyPressed(ImGuiKey_A))
            {
                selected_tab_ = 4;
                SetBuffer(alert_symbol_buffer_, sizeof(alert_symbol_buffer_), selected_symbol_);
            }
            for (int i = 0; i < 9; ++i)
            {
                const ImGuiKey key = static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_1) + i);
                if (ImGui::IsKeyPressed(key))
                    selected_tab_ = i;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_0))
                selected_tab_ = 9;
        }

        void AddSymbolFromBuffer()
        {
            std::vector<std::string> symbols = aegis::SplitWatchlist(config_.watchlist);
            const aegis::SymbolValidationResult symbol = aegis::ValidateTickerSymbol(symbol_buffer_);
            if (!symbol.ok)
            {
                status_ = "Ticker rejected: " + symbol.reason;
                return;
            }
            if (std::find(symbols.begin(), symbols.end(), symbol.symbol) == symbols.end())
                symbols.push_back(symbol.symbol);
            config_.watchlist = aegis::JoinWatchlist(symbols);
            SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
            SetBuffer(symbol_buffer_, sizeof(symbol_buffer_), "");
            aegis::SaveConfig(config_);
            selected_symbol_ = symbol.symbol;
            BeginRefresh(false);
            Audit("watchlist_added", symbol.symbol, "");
        }

        void RemoveSelectedSymbol()
        {
            std::vector<std::string> symbols = aegis::SplitWatchlist(config_.watchlist);
            const std::string target = aegis::Upper(selected_symbol_);
            symbols.erase(std::remove(symbols.begin(), symbols.end(), target), symbols.end());
            config_.watchlist = aegis::JoinWatchlist(symbols);
            SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
            aegis::SaveConfig(config_);
            if (!symbols.empty())
                selected_symbol_ = symbols.front();
            BeginRefresh(false);
            Audit("watchlist_removed", target, "");
        }

        void SyncConfigFromBuffers()
        {
            aegis::SettingsInput input;
            input.watchlist = watchlist_buffer_;
            input.alpha_vantage_api_key = api_key_buffer_;
            input.auth_base_url = auth_base_buffer_;
            input.sec_user_agent = sec_user_agent_buffer_;
            input.ui_light_theme = light_theme_;
            input.ui_compact_mode = compact_mode_;
            input.ui_high_contrast = config_.ui_high_contrast;
            input.font_scale_percent = config_.font_scale_percent;
            config_ = aegis::ApplySettingsInput(config_, input);
            aegis::SetHttpUserAgent(aegis::SecCompliantUserAgent(config_));
            SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
            SetBuffer(auth_base_buffer_, sizeof(auth_base_buffer_), config_.auth_base_url);
            SetBuffer(sec_user_agent_buffer_, sizeof(sec_user_agent_buffer_), config_.sec_user_agent);
        }

        void ApplyActiveTheme()
        {
            if (light_theme_)
                ApplyLightTheme();
            else
                ApplyTheme();
            if (config_.ui_high_contrast)
                ApplyHighContrastTheme(light_theme_);
        }

        void PersistUiPreferences(const std::string& saved_status)
        {
            config_.ui_light_theme = light_theme_;
            config_.ui_compact_mode = compact_mode_;
            config_.font_scale_percent = std::clamp(config_.font_scale_percent, 85, 150);
            ApplyActiveTheme();
            if (aegis::SaveConfig(config_))
                status_ = saved_status;
            else
                status_ = "Could not save UI preferences.";
        }

        void EnsureHoldingsInWatchlist()
        {
            std::vector<std::string> symbols = aegis::SplitWatchlist(config_.watchlist);
            bool changed = false;
            const auto add_symbol = [&](const std::string& raw_symbol) {
                const std::string symbol = aegis::NormalizeTickerSymbol(raw_symbol);
                if (symbol.empty())
                    return;
                if (std::find(symbols.begin(), symbols.end(), symbol) == symbols.end())
                {
                    symbols.push_back(symbol);
                    changed = true;
                }
            };
            for (const aegis::PortfolioHolding& holding : holdings_)
                add_symbol(holding.symbol);
            for (const aegis::PriceAlert& alert : price_alerts_)
                add_symbol(alert.symbol);
            for (const aegis::TradePlan& plan : trade_plans_)
                add_symbol(plan.symbol);
            if (changed)
                config_.watchlist = aegis::JoinWatchlist(symbols);
        }

        const aegis::StockQuote* FindExactQuote(const std::string& symbol) const
        {
            const std::string target = aegis::Upper(symbol);
            for (const aegis::StockQuote& quote : state_.quotes)
            {
                if (aegis::Upper(quote.symbol) == target)
                    return &quote;
            }
            return nullptr;
        }

        const aegis::StockSignal* FindExactSignal(const std::string& symbol) const
        {
            const std::string target = aegis::Upper(symbol);
            for (const aegis::StockSignal& signal : state_.signals)
            {
                if (aegis::Upper(signal.symbol) == target)
                    return &signal;
            }
            return nullptr;
        }

        PortfolioTotals CalculatePortfolioTotals() const
        {
            PortfolioTotals totals;
            for (const aegis::PortfolioHolding& holding : holdings_)
            {
                const aegis::StockQuote* quote = FindExactQuote(holding.symbol);
                const double mark = quote != nullptr && quote->price > 0.0 ? quote->price : holding.average_cost;
                totals.market_value += holding.shares * mark;
                totals.cost_basis += holding.shares * holding.average_cost;
            }
            totals.pnl = totals.market_value - totals.cost_basis;
            totals.pnl_percent = totals.cost_basis > 0.0 ? (totals.pnl / totals.cost_basis) * 100.0 : 0.0;
            return totals;
        }

        double PlanMarkPrice(const aegis::TradePlan& plan) const
        {
            const aegis::StockQuote* quote = FindExactQuote(plan.symbol);
            return quote != nullptr && quote->price > 0.0 ? quote->price : plan.entry;
        }

        double PlanUnrealizedPnl(const aegis::TradePlan& plan) const
        {
            return (PlanMarkPrice(plan) - plan.entry) * static_cast<double>(plan.shares);
        }

        std::string PlanProgressLabel(const aegis::TradePlan& plan) const
        {
            const double mark = PlanMarkPrice(plan);
            if (mark >= plan.target && plan.target > 0.0)
                return "Target hit";
            if (mark <= plan.stop && plan.stop > 0.0)
                return "Stop hit";
            if (plan.target > plan.entry && mark >= plan.entry)
                return "Advancing";
            if (mark < plan.entry)
                return "Under entry";
            return "Tracking";
        }

        TradePlanStats CalculateTradePlanStats() const
        {
            TradePlanStats stats;
            for (const aegis::TradePlan& plan : trade_plans_)
            {
                if (plan.status == "Active")
                    ++stats.active;
                else if (plan.status == "Closed")
                    ++stats.closed;
                else
                    ++stats.open;

                const double mark = PlanMarkPrice(plan);
                stats.open_notional += plan.entry * static_cast<double>(plan.shares);
                stats.mark_to_market += (mark - plan.entry) * static_cast<double>(plan.shares);
                if (plan.target > 0.0 && mark >= plan.target)
                    ++stats.target_hits;
                if (plan.stop > 0.0 && mark <= plan.stop)
                    ++stats.stop_hits;
            }
            return stats;
        }

        std::vector<ExposureRow> CalculateSectorExposure(double total_value) const
        {
            std::map<std::string, ExposureRow> rows;
            for (const aegis::PortfolioHolding& holding : holdings_)
            {
                const aegis::StockQuote* quote = FindExactQuote(holding.symbol);
                const double mark = quote != nullptr && quote->price > 0.0 ? quote->price : holding.average_cost;
                const std::string sector = quote != nullptr && !quote->sector.empty() ? quote->sector : "Unclassified";
                ExposureRow& row = rows[sector];
                row.name = sector;
                row.value += holding.shares * mark;
                row.count += 1;
            }

            std::vector<ExposureRow> out;
            out.reserve(rows.size());
            for (auto& pair : rows)
            {
                pair.second.percent = total_value > 0.0 ? (pair.second.value / total_value) * 100.0 : 0.0;
                out.push_back(pair.second);
            }
            std::sort(out.begin(), out.end(), [](const ExposureRow& a, const ExposureRow& b) {
                return a.value > b.value;
            });
            return out;
        }

        std::vector<ExposureRow> CalculatePositionExposure(double total_value) const
        {
            std::vector<ExposureRow> rows;
            rows.reserve(holdings_.size());
            for (const aegis::PortfolioHolding& holding : holdings_)
            {
                const aegis::StockQuote* quote = FindExactQuote(holding.symbol);
                const double mark = quote != nullptr && quote->price > 0.0 ? quote->price : holding.average_cost;
                ExposureRow row;
                row.name = holding.symbol;
                row.value = holding.shares * mark;
                row.percent = total_value > 0.0 ? (row.value / total_value) * 100.0 : 0.0;
                row.count = 1;
                rows.push_back(row);
            }
            std::sort(rows.begin(), rows.end(), [](const ExposureRow& a, const ExposureRow& b) {
                return a.value > b.value;
            });
            return rows;
        }

        void ClearHoldingEditor()
        {
            selected_holding_index_ = -1;
            SetBuffer(holding_symbol_buffer_, sizeof(holding_symbol_buffer_), "");
            SetBuffer(holding_note_buffer_, sizeof(holding_note_buffer_), "");
            holding_shares_ = 0.0;
            holding_average_cost_ = 0.0;
        }

        void LoadHoldingEditor(int index)
        {
            if (index < 0 || index >= static_cast<int>(holdings_.size()))
                return;
            selected_holding_index_ = index;
            const aegis::PortfolioHolding& holding = holdings_[static_cast<size_t>(index)];
            SetBuffer(holding_symbol_buffer_, sizeof(holding_symbol_buffer_), holding.symbol);
            SetBuffer(holding_note_buffer_, sizeof(holding_note_buffer_), holding.note);
            holding_shares_ = holding.shares;
            holding_average_cost_ = holding.average_cost;
            selected_symbol_ = holding.symbol;
        }

        void SaveHoldingFromEditor()
        {
            const aegis::HoldingDraftResult draft = aegis::ValidateHoldingDraft(holding_symbol_buffer_, holding_shares_, holding_average_cost_, holding_note_buffer_);
            if (!draft.ok)
            {
                status_ = "Holding rejected: " + draft.error;
                return;
            }
            const aegis::PortfolioHolding holding = draft.row;
            const int duplicate_index = aegis::FindDuplicateHoldingIndex(holdings_, holding.symbol, selected_holding_index_);

            if (selected_holding_index_ >= 0 && selected_holding_index_ < static_cast<int>(holdings_.size()))
            {
                if (duplicate_index >= 0)
                {
                    holdings_[static_cast<size_t>(duplicate_index)] = holding;
                    holdings_.erase(holdings_.begin() + selected_holding_index_);
                    selected_holding_index_ = duplicate_index > selected_holding_index_ ? duplicate_index - 1 : duplicate_index;
                }
                else
                {
                    holdings_[static_cast<size_t>(selected_holding_index_)] = holding;
                }
            }
            else
            {
                if (duplicate_index >= 0)
                {
                    holdings_[static_cast<size_t>(duplicate_index)] = holding;
                    selected_holding_index_ = duplicate_index;
                }
                else
                {
                    holdings_.push_back(holding);
                    selected_holding_index_ = static_cast<int>(holdings_.size()) - 1;
                }
            }

            std::sort(holdings_.begin(), holdings_.end(), [](const aegis::PortfolioHolding& a, const aegis::PortfolioHolding& b) {
                return a.symbol < b.symbol;
            });
            for (int i = 0; i < static_cast<int>(holdings_.size()); ++i)
            {
                if (holdings_[static_cast<size_t>(i)].symbol == holding.symbol)
                {
                    selected_holding_index_ = i;
                    break;
                }
            }

            selected_symbol_ = holding.symbol;
            if (aegis::SavePortfolioHoldings(holdings_))
                status_ = duplicate_index >= 0 ? "Holding saved and duplicate merged." : "Holding saved.";
            else
                status_ = "Could not save holdings.";
            Audit("holding_saved", holding.symbol, std::to_string(holding.shares) + " shares");
            EnsureHoldingsInWatchlist();
            SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
            aegis::SaveConfig(config_);
            BeginRefresh(false);
        }

        void RemoveSelectedHolding()
        {
            if (selected_holding_index_ < 0 || selected_holding_index_ >= static_cast<int>(holdings_.size()))
                return;
            const std::string removed = holdings_[static_cast<size_t>(selected_holding_index_)].symbol;
            holdings_.erase(holdings_.begin() + selected_holding_index_);
            selected_holding_index_ = -1;
            ClearHoldingEditor();
            if (aegis::SavePortfolioHoldings(holdings_))
                status_ = removed + " removed from holdings.";
            else
                status_ = "Could not save holdings.";
            Audit("holding_removed", removed, "");
        }

        void ClearAlertEditor()
        {
            selected_alert_index_ = -1;
            SetBuffer(alert_symbol_buffer_, sizeof(alert_symbol_buffer_), "");
            SetBuffer(alert_note_buffer_, sizeof(alert_note_buffer_), "");
            alert_trigger_price_ = 0.0;
            alert_above_ = true;
            alert_enabled_ = true;
        }

        void LoadAlertEditor(int index)
        {
            if (index < 0 || index >= static_cast<int>(price_alerts_.size()))
                return;
            selected_alert_index_ = index;
            const aegis::PriceAlert& alert = price_alerts_[static_cast<size_t>(index)];
            SetBuffer(alert_symbol_buffer_, sizeof(alert_symbol_buffer_), alert.symbol);
            SetBuffer(alert_note_buffer_, sizeof(alert_note_buffer_), alert.note);
            alert_trigger_price_ = alert.trigger_price;
            alert_above_ = alert.above;
            alert_enabled_ = alert.enabled;
            selected_symbol_ = alert.symbol;
        }

        void SaveAlertFromEditor()
        {
            const aegis::AlertDraftResult draft = aegis::ValidateAlertDraft(alert_symbol_buffer_, alert_trigger_price_, alert_above_, alert_enabled_, alert_note_buffer_);
            if (!draft.ok)
            {
                status_ = "Alert rejected: " + draft.error;
                return;
            }
            const aegis::PriceAlert alert = draft.row;
            const int duplicate_index = aegis::FindDuplicateAlertIndex(price_alerts_, alert, selected_alert_index_);

            if (selected_alert_index_ >= 0 && selected_alert_index_ < static_cast<int>(price_alerts_.size()))
            {
                if (duplicate_index >= 0)
                {
                    price_alerts_[static_cast<size_t>(duplicate_index)] = alert;
                    price_alerts_.erase(price_alerts_.begin() + selected_alert_index_);
                    selected_alert_index_ = duplicate_index > selected_alert_index_ ? duplicate_index - 1 : duplicate_index;
                }
                else
                {
                    price_alerts_[static_cast<size_t>(selected_alert_index_)] = alert;
                }
            }
            else
            {
                if (duplicate_index >= 0)
                {
                    price_alerts_[static_cast<size_t>(duplicate_index)] = alert;
                    selected_alert_index_ = duplicate_index;
                }
                else
                {
                    price_alerts_.push_back(alert);
                    selected_alert_index_ = static_cast<int>(price_alerts_.size()) - 1;
                }
            }
            std::sort(price_alerts_.begin(), price_alerts_.end(), [](const aegis::PriceAlert& a, const aegis::PriceAlert& b) {
                if (a.symbol != b.symbol)
                    return a.symbol < b.symbol;
                return a.trigger_price < b.trigger_price;
            });
            for (int i = 0; i < static_cast<int>(price_alerts_.size()); ++i)
            {
                const aegis::PriceAlert& row = price_alerts_[static_cast<size_t>(i)];
                if (row.symbol == alert.symbol && row.trigger_price == alert.trigger_price && row.above == alert.above)
                {
                    selected_alert_index_ = i;
                    break;
                }
            }
            selected_symbol_ = alert.symbol;
            if (aegis::SavePriceAlerts(price_alerts_))
                status_ = duplicate_index >= 0 ? "Price alert saved and duplicate merged." : "Price alert saved.";
            else
                status_ = "Could not save price alerts.";
            Audit("alert_saved", alert.symbol, (alert.above ? "above " : "below ") + aegis::FormatCurrency(alert.trigger_price));
            EnsureHoldingsInWatchlist();
            SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
            aegis::SaveConfig(config_);
            EvaluateCustomAlerts();
            BeginRefresh(false);
        }

        void RemoveSelectedAlert()
        {
            if (selected_alert_index_ < 0 || selected_alert_index_ >= static_cast<int>(price_alerts_.size()))
                return;
            price_alerts_.erase(price_alerts_.begin() + selected_alert_index_);
            selected_alert_index_ = -1;
            ClearAlertEditor();
            if (aegis::SavePriceAlerts(price_alerts_))
                status_ = "Price alert removed.";
            else
                status_ = "Could not save price alerts.";
            Audit("alert_removed", "", "");
            EvaluateCustomAlerts();
        }

        double ReturnCorrelation(const aegis::StockQuote* a, const aegis::StockQuote* b) const
        {
            if (a == nullptr || b == nullptr || a->history.size() < 3 || b->history.size() < 3)
                return 0.0;
            const size_t count = std::min(a->history.size(), b->history.size());
            std::vector<double> ar;
            std::vector<double> br;
            ar.reserve(count - 1);
            br.reserve(count - 1);
            for (size_t i = 1; i < count; ++i)
            {
                const float a0 = a->history[a->history.size() - count + i - 1];
                const float a1 = a->history[a->history.size() - count + i];
                const float b0 = b->history[b->history.size() - count + i - 1];
                const float b1 = b->history[b->history.size() - count + i];
                if (a0 > 0.0f && b0 > 0.0f)
                {
                    ar.push_back((static_cast<double>(a1) - static_cast<double>(a0)) / static_cast<double>(a0));
                    br.push_back((static_cast<double>(b1) - static_cast<double>(b0)) / static_cast<double>(b0));
                }
            }
            if (ar.size() < 2 || br.size() != ar.size())
                return 0.0;
            double am = 0.0;
            double bm = 0.0;
            for (size_t i = 0; i < ar.size(); ++i)
            {
                am += ar[i];
                bm += br[i];
            }
            am /= static_cast<double>(ar.size());
            bm /= static_cast<double>(br.size());
            double cov = 0.0;
            double av = 0.0;
            double bv = 0.0;
            for (size_t i = 0; i < ar.size(); ++i)
            {
                const double ad = ar[i] - am;
                const double bd = br[i] - bm;
                cov += ad * bd;
                av += ad * ad;
                bv += bd * bd;
            }
            if (av <= 0.0 || bv <= 0.0)
                return 0.0;
            return std::clamp(cov / std::sqrt(av * bv), -1.0, 1.0);
        }

        double ReturnBeta(const aegis::StockQuote* quote, const aegis::StockQuote* benchmark) const
        {
            const double corr = ReturnCorrelation(quote, benchmark);
            if (quote == nullptr || benchmark == nullptr || quote->history.size() < 3 || benchmark->history.size() < 3)
                return quote != nullptr ? quote->beta : 1.0;

            const auto volatility = [](const std::vector<float>& history) {
                std::vector<double> returns;
                for (size_t i = 1; i < history.size(); ++i)
                {
                    if (history[i - 1] > 0.0f)
                        returns.push_back((static_cast<double>(history[i]) - static_cast<double>(history[i - 1])) / static_cast<double>(history[i - 1]));
                }
                if (returns.size() < 2)
                    return 0.0;
                double avg = 0.0;
                for (double value : returns)
                    avg += value;
                avg /= static_cast<double>(returns.size());
                double sum = 0.0;
                for (double value : returns)
                    sum += (value - avg) * (value - avg);
                return std::sqrt(sum / static_cast<double>(returns.size() - 1));
            };

            const double qv = volatility(quote->history);
            const double bv = volatility(benchmark->history);
            if (bv <= 0.0)
                return quote->beta;
            return std::clamp(corr * (qv / bv), -3.0, 3.0);
        }

        void EvaluateCustomAlerts(const char* reason = "manual")
        {
            state_.alerts.erase(std::remove_if(state_.alerts.begin(), state_.alerts.end(), [](const aegis::InfoItem& item) {
                return item.tag == "Custom";
            }), state_.alerts.end());

            const aegis::AlertEvaluationResult evaluation = aegis::EvaluatePriceAlerts(price_alerts_, state_.quotes, alert_events_, state_.source_badge);
            state_.alerts.insert(state_.alerts.end(), evaluation.rows.begin(), evaluation.rows.end());
            last_alert_check_ = std::chrono::steady_clock::now();
            alert_monitor_status_ = evaluation.summary;

            if (!evaluation.new_events.empty())
            {
                for (const aegis::AlertEvent& event : evaluation.new_events)
                {
                    alert_events_.push_back(event);
                    aegis::AppendDiagnosticEvent({ "info", "alert-engine", reason, event.symbol, "triggered", event.direction + " " + aegis::FormatCurrency(event.trigger_price), "", 0, 0, false });
                }
                std::sort(alert_events_.begin(), alert_events_.end(), [](const aegis::AlertEvent& a, const aegis::AlertEvent& b) {
                    return a.triggered_at < b.triggered_at;
                });
                aegis::SaveAlertEvents(alert_events_);
            }
        }

        void RenderPriceAlertsPanel()
        {
            SectionTitle("Custom Price Alerts", "Saved alerts are monitored on a schedule against the current quote board.");
            TextMuted(alert_monitor_status_.c_str());
            if (SmallActionButton("Check Alerts Now"))
                EvaluateCustomAlerts("manual");
            ImGui::SameLine();
            const int unread_alerts = aegis::CountUnreadAlertEvents(alert_events_);
            ImGui::TextWrapped("%d unread alert%s", unread_alerts, unread_alerts == 1 ? "" : "s");
            ImGui::SetNextItemWidth(95.0f);
            ImGui::InputText("Symbol##alert", alert_symbol_buffer_, sizeof(alert_symbol_buffer_));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(116.0f);
            ImGui::InputDouble("Price##alert", &alert_trigger_price_, 1.0, 10.0, "%.2f");
            int direction = alert_above_ ? 0 : 1;
            ImGui::SetNextItemWidth(140.0f);
            if (ImGui::Combo("Direction", &direction, "Above\0Below\0"))
                alert_above_ = direction == 0;
            ImGui::SameLine();
            ImGui::Checkbox("Enabled", &alert_enabled_);
            ImGui::InputTextMultiline("Note##alert", alert_note_buffer_, sizeof(alert_note_buffer_), ImVec2(0, 58.0f));
            if (SmallActionButton(selected_alert_index_ >= 0 ? "Update Alert" : "Add Alert"))
                SaveAlertFromEditor();
            ImGui::SameLine();
            if (SmallActionButton("Clear Alert"))
                ClearAlertEditor();
            if (selected_alert_index_ >= 0)
            {
                ImGui::SameLine();
                if (SmallActionButton("Remove Alert"))
                    RemoveSelectedAlert();
            }

            if (ImGui::BeginTable("custom_alerts", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 180.0f)))
            {
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 78.0f);
                ImGui::TableSetupColumn("Rule", ImGuiTableColumnFlags_WidthFixed, 116.0f);
                ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 48.0f);
                ImGui::TableSetupColumn("Note");
                ImGui::TableHeadersRow();

                for (int i = 0; i < static_cast<int>(price_alerts_.size()); ++i)
                {
                    const aegis::PriceAlert& alert = price_alerts_[static_cast<size_t>(i)];
                    const aegis::StockQuote* quote = FindExactQuote(alert.symbol);
                    const double last = quote != nullptr ? quote->price : 0.0;
                    const bool triggered = quote != nullptr && aegis::IsPriceAlertTriggered(alert, *quote);
                    const std::string rule = std::string(alert.above ? "> " : "< ") + aegis::FormatCurrency(alert.trigger_price);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(alert.symbol.c_str(), selected_alert_index_ == i, ImGuiSelectableFlags_SpanAllColumns))
                        LoadAlertEditor(i);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(rule.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(last > 0.0 ? aegis::FormatCurrency(last).c_str() : "--");
                    ImGui::TableNextColumn();
                    TextValueColored(triggered ? 1.0 : -1.0, triggered ? "Triggered" : "Watching");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(alert.enabled ? "Yes" : "No");
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", alert.note.c_str());
                }
                ImGui::EndTable();
            }
        }

        void RenderHoldingsTable(const PortfolioTotals& totals)
        {
            SectionTitle("Saved Holdings", holdings_.empty() ? "Add a holding to track allocation and P/L." : "Click a row to edit it.");
            if (ImGui::BeginTable("holdings", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 0)))
            {
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Shares", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 102.0f);
                ImGui::TableSetupColumn("P/L", ImGuiTableColumnFlags_WidthFixed, 96.0f);
                ImGui::TableSetupColumn("P/L %", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Alloc", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Note");
                ImGui::TableHeadersRow();

                for (int i = 0; i < static_cast<int>(holdings_.size()); ++i)
                {
                    const aegis::PortfolioHolding& holding = holdings_[static_cast<size_t>(i)];
                    const aegis::StockQuote* quote = FindExactQuote(holding.symbol);
                    const double last = quote != nullptr && quote->price > 0.0 ? quote->price : holding.average_cost;
                    const double value = holding.shares * last;
                    const double cost = holding.shares * holding.average_cost;
                    const double pnl = value - cost;
                    const double pnl_percent = cost > 0.0 ? (pnl / cost) * 100.0 : 0.0;
                    const double allocation = totals.market_value > 0.0 ? (value / totals.market_value) * 100.0 : 0.0;

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(holding.symbol.c_str(), selected_holding_index_ == i, ImGuiSelectableFlags_SpanAllColumns))
                        LoadHoldingEditor(i);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.4f", holding.shares);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(holding.average_cost).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(last).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(value).c_str());
                    ImGui::TableNextColumn();
                    TextValueColored(pnl, aegis::FormatCurrency(pnl));
                    ImGui::TableNextColumn();
                    TextValueColored(pnl_percent, aegis::FormatPercent(pnl_percent));
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatPercent(allocation).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", holding.note.c_str());
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();
            RenderExposureTables(totals);
        }

        void RenderExposureTables(const PortfolioTotals& totals)
        {
            const std::vector<ExposureRow> sectors = CalculateSectorExposure(totals.market_value);
            const std::vector<ExposureRow> positions = CalculatePositionExposure(totals.market_value);

            ImGui::BeginChild("sector_exposure", ImVec2(ImGui::GetContentRegionAvail().x * 0.50f - 6.0f, 150.0f), true);
            SectionTitle("Sector Exposure");
            if (ImGui::BeginTable("sector_exposure_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Sector");
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 96.0f);
                ImGui::TableSetupColumn("Alloc", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                ImGui::TableSetupColumn("Names", ImGuiTableColumnFlags_WidthFixed, 62.0f);
                ImGui::TableHeadersRow();
                for (const ExposureRow& row : sectors)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(row.value).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatPercent(row.percent).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", row.count);
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("position_exposure", ImVec2(0, 150.0f), true);
            SectionTitle("Concentration");
            if (ImGui::BeginTable("position_exposure_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 96.0f);
                ImGui::TableSetupColumn("Alloc", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                ImGui::TableSetupColumn("Flag");
                ImGui::TableHeadersRow();
                for (const ExposureRow& row : positions)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(row.name.c_str(), selected_symbol_ == row.name, ImGuiSelectableFlags_SpanAllColumns))
                        selected_symbol_ = row.name;
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(row.value).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatPercent(row.percent).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row.percent >= 25.0 ? "High" : row.percent >= 15.0 ? "Watch" : "OK");
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }

        bool CurrentSizing(int& shares, double& notional, double& planned_risk, double& planned_reward) const
        {
            const aegis::StockQuote* quote = aegis::FindQuote(state_, selected_symbol_);
            const aegis::StockSignal* signal = aegis::FindSignal(state_, selected_symbol_);
            if (quote == nullptr || signal == nullptr || worksheet_entry_ <= 0.0)
                return false;

            double atr14 = 0.0;
            if (sizing_mode_ == 3)
            {
                const std::vector<aegis::Candle> candles = aegis::BuildSyntheticCandles(*quote, 90);
                const aegis::IndicatorSnapshot indicators = aegis::BuildIndicators(candles, FindExactQuote("SPY"), FindExactQuote("QQQ"));
                atr14 = indicators.atr14;
            }

            aegis::PositionSizingInput input;
            input.mode = aegis::PositionSizingModeFromIndex(sizing_mode_);
            input.portfolio_cash = config_.portfolio_cash;
            input.max_position_amount = config_.max_position_amount;
            input.max_portfolio_risk_percent = config_.max_portfolio_risk_percent;
            input.entry = worksheet_entry_;
            input.stop = worksheet_stop_;
            input.target = signal->target_price;
            input.beta = quote->beta;
            input.change_percent = quote->change_percent;
            input.score = signal->score;
            input.atr14 = atr14;
            const aegis::PositionSizingResult result = aegis::CalculatePositionSize(input);
            shares = result.shares;
            notional = result.notional;
            planned_risk = result.planned_risk;
            planned_reward = result.planned_reward;
            return result.ok;
        }

        void SaveTradePlanFromWorksheet()
        {
            const aegis::StockQuote* quote = aegis::FindQuote(state_, selected_symbol_);
            const aegis::StockSignal* signal = aegis::FindSignal(state_, selected_symbol_);
            if (quote == nullptr || signal == nullptr)
            {
                status_ = "Select a symbol before saving a trade plan.";
                return;
            }

            int shares = 0;
            double notional = 0.0;
            double planned_risk = 0.0;
            double planned_reward = 0.0;
            if (!CurrentSizing(shares, notional, planned_risk, planned_reward))
            {
                status_ = "Sizing produced zero shares. Check entry, stop, and risk limits.";
                return;
            }

            aegis::TradePlan plan;
            plan.created_at = aegis::NowTimeLabel();
            plan.symbol = quote->symbol;
            plan.rating = signal->rating;
            plan.entry = worksheet_entry_;
            plan.stop = worksheet_stop_;
            plan.target = signal->target_price;
            plan.shares = shares;
            plan.planned_risk = planned_risk;
            plan.planned_reward = planned_reward;
            plan.thesis = signal->thesis;
            plan.status = "Open";
            const aegis::TradePlanDraftResult draft = aegis::ValidateTradePlanDraft(plan);
            if (!draft.ok)
            {
                status_ = "Trade plan rejected: " + draft.error;
                return;
            }
            trade_plans_.push_back(draft.row);
            selected_trade_plan_index_ = static_cast<int>(trade_plans_.size()) - 1;
            if (aegis::SaveTradePlans(trade_plans_))
                status_ = quote->symbol + " trade plan saved.";
            else
                status_ = "Could not save trade plan.";
            Audit("trade_plan_saved", draft.row.symbol, draft.row.rating);
            EnsureHoldingsInWatchlist();
            SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
            aegis::SaveConfig(config_);
        }

        void RemoveSelectedTradePlan()
        {
            if (selected_trade_plan_index_ < 0 || selected_trade_plan_index_ >= static_cast<int>(trade_plans_.size()))
                return;
            const std::string symbol = trade_plans_[static_cast<size_t>(selected_trade_plan_index_)].symbol;
            trade_plans_.erase(trade_plans_.begin() + selected_trade_plan_index_);
            selected_trade_plan_index_ = -1;
            if (aegis::SaveTradePlans(trade_plans_))
                status_ = symbol + " trade plan removed.";
            else
                status_ = "Could not save trade plans.";
            Audit("trade_plan_removed", symbol, "");
        }

        void CycleSelectedTradePlanStatus()
        {
            if (selected_trade_plan_index_ < 0 || selected_trade_plan_index_ >= static_cast<int>(trade_plans_.size()))
                return;
            aegis::TradePlan& plan = trade_plans_[static_cast<size_t>(selected_trade_plan_index_)];
            if (plan.status == "Open")
                plan.status = "Active";
            else if (plan.status == "Active")
                plan.status = "Closed";
            else
                plan.status = "Open";
            if (aegis::SaveTradePlans(trade_plans_))
                status_ = plan.symbol + " trade plan marked " + plan.status + ".";
            else
                status_ = "Could not save trade plan status.";
            Audit("trade_plan_status", plan.symbol, plan.status);
        }

        void AddSelectedTradePlanToHoldings()
        {
            if (selected_trade_plan_index_ < 0 || selected_trade_plan_index_ >= static_cast<int>(trade_plans_.size()))
                return;

            aegis::TradePlan& plan = trade_plans_[static_cast<size_t>(selected_trade_plan_index_)];
            if (plan.shares <= 0 || plan.entry <= 0.0)
            {
                status_ = "Selected plan has no valid shares or entry.";
                return;
            }

            auto existing = std::find_if(holdings_.begin(), holdings_.end(), [&](const aegis::PortfolioHolding& holding) {
                return aegis::Upper(holding.symbol) == aegis::Upper(plan.symbol);
            });

            if (existing != holdings_.end())
            {
                const double old_cost = existing->average_cost * existing->shares;
                const double add_cost = plan.entry * static_cast<double>(plan.shares);
                existing->shares += static_cast<double>(plan.shares);
                existing->average_cost = existing->shares > 0.0 ? (old_cost + add_cost) / existing->shares : plan.entry;
                if (existing->note.empty())
                    existing->note = "Added from trade plan";
            }
            else
            {
                holdings_.push_back({ plan.symbol, static_cast<double>(plan.shares), plan.entry, "Added from trade plan" });
            }

            plan.status = "Active";
            std::sort(holdings_.begin(), holdings_.end(), [](const aegis::PortfolioHolding& a, const aegis::PortfolioHolding& b) {
                return a.symbol < b.symbol;
            });
            aegis::SavePortfolioHoldings(holdings_);
            aegis::SaveTradePlans(trade_plans_);
            EnsureHoldingsInWatchlist();
            SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
            status_ = plan.symbol + " added to holdings from trade plan.";
            Audit("plan_promoted_to_holding", plan.symbol, std::to_string(plan.shares) + " shares");
        }

        void RenderTradePlansTable()
        {
            const TradePlanStats stats = CalculateTradePlanStats();
            SectionTitle("Trade Plans", trade_plans_.empty() ? "Save a sizing worksheet to create a paper plan." : "Click a plan to select it.");
            ImGui::Text("Open %d   Active %d   Closed %d   Target hits %d   Stop hits %d   Plan P/L %s",
                stats.open,
                stats.active,
                stats.closed,
                stats.target_hits,
                stats.stop_hits,
                aegis::FormatCurrency(stats.mark_to_market).c_str());
            if (selected_trade_plan_index_ >= 0)
            {
                ImGui::SameLine();
                if (SmallActionButton("Add to Holdings"))
                    AddSelectedTradePlanToHoldings();
                ImGui::SameLine();
                if (SmallActionButton("Cycle Status"))
                    CycleSelectedTradePlanStatus();
                ImGui::SameLine();
                if (SmallActionButton("Remove Plan"))
                    RemoveSelectedTradePlan();
            }

            if (ImGui::BeginTable("trade_plans_table", 13, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX, ImVec2(0, 0)))
            {
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 78.0f);
                ImGui::TableSetupColumn("Rating", ImGuiTableColumnFlags_WidthFixed, 106.0f);
                ImGui::TableSetupColumn("Entry", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Mark", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("P/L", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Stop", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 98.0f);
                ImGui::TableSetupColumn("Shares", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                ImGui::TableSetupColumn("R/R", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                ImGui::TableSetupColumn("Thesis");
                ImGui::TableHeadersRow();

                for (int i = 0; i < static_cast<int>(trade_plans_.size()); ++i)
                {
                    const aegis::TradePlan& plan = trade_plans_[static_cast<size_t>(i)];
                    const double rr = plan.planned_risk > 0.0 ? plan.planned_reward / plan.planned_risk : 0.0;
                    const double mark = PlanMarkPrice(plan);
                    const double pnl = PlanUnrealizedPnl(plan);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(plan.created_at.c_str());
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(plan.symbol.c_str(), selected_trade_plan_index_ == i, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        selected_trade_plan_index_ = i;
                        selected_symbol_ = plan.symbol;
                        worksheet_entry_ = plan.entry;
                        worksheet_stop_ = plan.stop;
                        selected_symbol_for_worksheet_ = plan.symbol;
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(plan.status.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(plan.rating.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(plan.entry).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(mark).c_str());
                    ImGui::TableNextColumn();
                    TextValueColored(pnl, aegis::FormatCurrency(pnl));
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(plan.stop).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(plan.target).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(PlanProgressLabel(plan).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", plan.shares);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", rr);
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", plan.thesis.c_str());
                }
                ImGui::EndTable();
            }
        }

        void RenderSizingWorksheet()
        {
            const aegis::StockQuote* quote = aegis::FindQuote(state_, selected_symbol_);
            const aegis::StockSignal* signal = aegis::FindSignal(state_, selected_symbol_);
            if (quote == nullptr || signal == nullptr)
            {
                TextMuted("Select a symbol to size a paper entry.");
                return;
            }

            if (worksheet_entry_ <= 0.0 || selected_symbol_for_worksheet_ != quote->symbol)
            {
                worksheet_entry_ = quote->price;
                worksheet_stop_ = signal->stop_level;
                selected_symbol_for_worksheet_ = quote->symbol;
            }
            SectionTitle((quote->symbol + " Sizing").c_str(), signal->rating.c_str());
            ImGui::SetNextItemWidth(190.0f);
            ImGui::Combo("Sizing mode", &sizing_mode_, "Fixed dollar\0Fixed risk\0Vol-adjusted\0ATR stop\0Kelly-lite\0");
            ImGui::InputDouble("Entry", &worksheet_entry_, 0.1, 1.0, "%.2f");
            ImGui::InputDouble("Stop", &worksheet_stop_, 0.1, 1.0, "%.2f");
            if (sizing_mode_ == 3)
            {
                ImGui::SameLine();
                if (SmallActionButton("Use ATR Stop"))
                {
                    const std::vector<aegis::Candle> candles = aegis::BuildSyntheticCandles(*quote, 90);
                    const aegis::IndicatorSnapshot indicators = aegis::BuildIndicators(candles, FindExactQuote("SPY"), FindExactQuote("QQQ"));
                    worksheet_stop_ = std::max(0.01, worksheet_entry_ - indicators.atr14 * 1.5);
                }
            }
            int shares = 0;
            double notional = 0.0;
            double planned_risk = 0.0;
            double planned_reward = 0.0;
            CurrentSizing(shares, notional, planned_risk, planned_reward);

            ImGui::Spacing();
            MetricCard({"Shares", "", std::to_string(shares), "", "Lower of risk-budget and position-cap sizing."}, ImVec2(168.0f, 104.0f));
            ImGui::SameLine();
            MetricCard({"Notional", "", aegis::FormatCurrency(notional), "", "Estimated paper trade value."}, ImVec2(168.0f, 104.0f));
            ImGui::SameLine();
            MetricCard({"Risk", "", aegis::FormatCurrency(planned_risk), "", "Entry minus stop, multiplied by shares."}, ImVec2(168.0f, 104.0f));
            ImGui::SameLine();
            MetricCard({"Reward", "", aegis::FormatCurrency(planned_reward), "", "Target minus entry, multiplied by shares."}, ImVec2(168.0f, 104.0f));

            ImGui::Spacing();
            if (SmallActionButton("Save Trade Plan"))
                SaveTradePlanFromWorksheet();
            ImGui::SameLine();
            if (SmallActionButton("Open Symbol Research"))
                selected_tab_ = 4;

            ImGui::Spacing();
            SectionTitle("Eligible Signals");
            if (ImGui::BeginTable("portfolio_signals", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Rating", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Budget", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("Risk", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Thesis");
                ImGui::TableHeadersRow();
                for (const aegis::StockSignal& row : state_.signals)
                {
                    if (row.confidence < config_.min_conviction)
                        continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(row.symbol.c_str(), selected_symbol_ == row.symbol, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        selected_symbol_ = row.symbol;
                        worksheet_entry_ = 0.0;
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row.rating.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(aegis::FormatCurrency(row.position_budget).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row.risk.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", row.thesis.c_str());
                }
                ImGui::EndTable();
            }
        }

        std::string QuoteFilterKey() const
        {
            static const char* keys[] = { "all", "gainers", "losers", "etfs", "technology", "semiconductors" };
            return keys[std::clamp(quote_filter_, 0, 5)];
        }

        std::string SignalFilterKey() const
        {
            static const char* keys[] = { "all", "eligible", "bullish", "risk", "hold", "wait" };
            return keys[std::clamp(signal_filter_, 0, 5)];
        }

        void BeginRefresh(bool initial)
        {
            if (refresh_in_flight_)
            {
                status_ = "Refresh already running.";
                return;
            }
            SyncConfigFromBuffers();
            EnsureHoldingsInWatchlist();
            SetBuffer(watchlist_buffer_, sizeof(watchlist_buffer_), config_.watchlist);
            const int request_id = ++refresh_request_id_;
            const aegis::Config config = config_;
            refresh_in_flight_ = true;
            status_ = initial ? "Preparing market board..." : "Refreshing quotes...";
            refresh_future_ = std::async(std::launch::async, [request_id, config]() {
                RefreshResult result;
                result.request_id = request_id;
                result.ok = true;
                result.state = aegis::BuildNativeStockState(config);
                result.status = "Market board refreshed at " + result.state.last_refresh_label + ".";
                result.diagnostic = "stock refresh quotes=" + std::to_string(result.state.quotes.size()) + " signals=" + std::to_string(result.state.signals.size()) + " source=" + result.state.source_badge;
                return result;
            });
        }

        void PollRefresh()
        {
            if (!refresh_in_flight_ || !refresh_future_.valid())
                return;
            if (refresh_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
                return;

            RefreshResult result;
            try
            {
                result = refresh_future_.get();
            }
            catch (const std::exception& ex)
            {
                result.ok = false;
                result.status = std::string("Refresh failed: ") + ex.what();
                result.diagnostic = std::string("stock refresh exception: ") + ex.what();
            }
            catch (...)
            {
                result.ok = false;
                result.status = "Refresh failed by an unknown error.";
                result.diagnostic = "stock refresh unknown exception";
            }

            refresh_in_flight_ = false;
            if (result.request_id != 0 && result.request_id != refresh_request_id_)
                return;
            if (!result.diagnostic.empty())
                aegis::AppendDiagnosticLine(result.diagnostic);
            if (result.ok)
            {
                state_ = std::move(result.state);
                EvaluateCustomAlerts();
                if (aegis::FindQuote(state_, selected_symbol_) == nullptr)
                    selected_symbol_ = state_.selected_symbol;
                status_ = result.status;
                last_refresh_ = std::chrono::steady_clock::now();
            }
            else
            {
                status_ = result.status;
            }
        }

        void MaybeAutoRefresh()
        {
            if (refresh_in_flight_)
                return;
            const int seconds = std::max(30, config_.refresh_seconds);
            if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - last_refresh_).count() >= seconds)
                BeginRefresh(false);
        }

        void MaybeAlertMonitor()
        {
            int enabled_alerts = 0;
            for (const aegis::PriceAlert& alert : price_alerts_)
            {
                if (alert.enabled)
                    ++enabled_alerts;
            }
            if (enabled_alerts == 0)
            {
                alert_monitor_status_ = price_alerts_.empty() ? "Alert monitor idle: no saved alerts." : "Alert monitor idle: saved alerts are disabled.";
                return;
            }
            const int interval = std::clamp(config_.refresh_seconds / 2, 30, 300);
            const auto now = std::chrono::steady_clock::now();
            if (last_alert_check_.time_since_epoch().count() != 0 &&
                std::chrono::duration_cast<std::chrono::seconds>(now - last_alert_check_).count() < interval)
                return;
            EvaluateCustomAlerts("scheduled");
        }

        void BeginValidation()
        {
            if (validation_in_flight_)
                return;
            SyncConfigFromBuffers();
            const int request_id = ++validation_request_id_;
            validation_in_flight_ = true;
            validation_status_ = "Validating Alpha Vantage key...";
            const std::string key = config_.alpha_vantage_api_key;
            validation_future_ = std::async(std::launch::async, [request_id, key]() {
                ValidationFutureResult result;
                result.request_id = request_id;
                result.validation = aegis::ValidateAlphaVantageKey(key);
                result.status = result.validation.status + ": " + result.validation.detail;
                return result;
            });
        }

        void PollValidation()
        {
            if (!validation_in_flight_ || !validation_future_.valid())
                return;
            if (validation_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
                return;

            ValidationFutureResult result;
            result.request_id = validation_request_id_;
            try
            {
                result = validation_future_.get();
            }
            catch (const std::exception& ex)
            {
                result.status = std::string("Validation failed: ") + ex.what();
            }
            catch (...)
            {
                result.status = "Validation failed by an unknown error.";
            }
            validation_in_flight_ = false;
            if (result.request_id != validation_request_id_)
            {
                validation_status_ = "Ignored stale validation response.";
                aegis::AppendDiagnosticEvent({ "info", "alpha-vantage", "validation", "", "stale", "Ignored stale validation response after a newer request.", "", 0, 0, false });
                return;
            }
            validation_status_ = result.status;
            aegis::AppendDiagnosticLine("alpha validation " + result.status);
        }

        void EnsureHistoryLoad(const aegis::StockQuote& quote)
        {
            if (quote.symbol.empty())
                return;
            const int requested_days = std::clamp(chart_days_, 30, 1260);
            if (history_symbol_ == quote.symbol && !history_result_.candles.empty() && history_requested_days_ >= requested_days)
                return;
            if (history_in_flight_)
            {
                if (history_request_symbol_ != quote.symbol || history_in_flight_days_ < requested_days)
                {
                    ++history_request_id_;
                    history_request_symbol_ = quote.symbol;
                    history_in_flight_days_ = requested_days;
                    history_status_ = "Queued " + quote.symbol + " candles; older response will be ignored.";
                }
                return;
            }

            const int request_id = ++history_request_id_;
            history_request_symbol_ = quote.symbol;
            history_in_flight_days_ = requested_days;
            history_symbol_.clear();
            history_result_ = {};
            history_status_ = "Loading " + quote.symbol + " daily candles (" + std::to_string(requested_days) + " days)...";
            const aegis::Config config = config_;
            const aegis::StockQuote request_quote = quote;
            history_in_flight_ = true;
            history_future_ = std::async(std::launch::async, [request_id, requested_days, config, request_quote]() {
                HistoryFutureResult wrapped;
                wrapped.request_id = request_id;
                wrapped.requested_days = requested_days;
                wrapped.symbol = request_quote.symbol;
                wrapped.result = aegis::LoadHistoricalCandles(config, request_quote, requested_days);
                return wrapped;
            });
        }

        void PollHistoryLoad()
        {
            if (!history_in_flight_ || !history_future_.valid())
                return;
            if (history_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
                return;

            HistoryFutureResult wrapped;
            wrapped.request_id = history_request_id_;
            wrapped.requested_days = history_in_flight_days_;
            wrapped.symbol = history_request_symbol_;
            try
            {
                wrapped = history_future_.get();
            }
            catch (const std::exception& ex)
            {
                wrapped.result.ok = false;
                wrapped.result.status = std::string("Historical candle load failed: ") + ex.what();
            }
            catch (...)
            {
                wrapped.result.ok = false;
                wrapped.result.status = "Historical candle load failed by an unknown error.";
            }

            history_in_flight_ = false;
            if (wrapped.request_id != history_request_id_ || (!wrapped.symbol.empty() && wrapped.symbol != history_request_symbol_))
            {
                history_in_flight_days_ = 0;
                history_status_ = "Ignored stale history response.";
                aegis::AppendDiagnosticEvent({ "info", "history", "TIME_SERIES_DAILY", wrapped.symbol, "stale", "Ignored stale history response after symbol switch.", "", 0, 0, false });
                return;
            }
            history_symbol_ = wrapped.symbol.empty() ? history_request_symbol_ : wrapped.symbol;
            history_result_ = std::move(wrapped.result);
            history_requested_days_ = std::max(wrapped.requested_days, static_cast<int>(history_result_.candles.size()));
            history_in_flight_days_ = 0;
            history_status_ = history_result_.status;
            if (!history_result_.status.empty())
                aegis::AppendDiagnosticLine("history " + history_symbol_ + " " + history_result_.status);
        }

        void EnsureResearchLoad(const aegis::StockQuote& quote)
        {
            if (quote.symbol.empty())
                return;
            if (research_symbol_ == quote.symbol && research_result_.ok)
                return;
            if (research_in_flight_)
            {
                if (research_request_symbol_ != quote.symbol)
                {
                    ++research_request_id_;
                    research_request_symbol_ = quote.symbol;
                    research_status_ = "Queued " + quote.symbol + " research; older response will be ignored.";
                }
                return;
            }

            const int request_id = ++research_request_id_;
            research_request_symbol_ = quote.symbol;
            research_symbol_.clear();
            research_result_ = {};
            research_status_ = "Loading " + quote.symbol + " fundamentals/news/earnings...";
            const aegis::Config config = config_;
            const aegis::StockQuote request_quote = quote;
            research_in_flight_ = true;
            research_future_ = std::async(std::launch::async, [request_id, config, request_quote]() {
                ResearchFutureResult wrapped;
                wrapped.request_id = request_id;
                wrapped.symbol = request_quote.symbol;
                wrapped.result = aegis::LoadResearchBundle(config, request_quote);
                return wrapped;
            });
        }

        void PollResearchLoad()
        {
            if (!research_in_flight_ || !research_future_.valid())
                return;
            if (research_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
                return;

            ResearchFutureResult wrapped;
            wrapped.request_id = research_request_id_;
            wrapped.symbol = research_request_symbol_;
            try
            {
                wrapped = research_future_.get();
            }
            catch (const std::exception& ex)
            {
                wrapped.result.ok = false;
                wrapped.result.status = std::string("Research bundle load failed: ") + ex.what();
            }
            catch (...)
            {
                wrapped.result.ok = false;
                wrapped.result.status = "Research bundle load failed by an unknown error.";
            }

            research_in_flight_ = false;
            if (wrapped.request_id != research_request_id_ || (!wrapped.symbol.empty() && wrapped.symbol != research_request_symbol_))
            {
                research_status_ = "Ignored stale research response.";
                aegis::AppendDiagnosticEvent({ "info", "research", "bundle", wrapped.symbol, "stale", "Ignored stale research response after symbol switch.", "", 0, 0, false });
                return;
            }
            research_symbol_ = wrapped.symbol.empty() ? research_request_symbol_ : wrapped.symbol;
            research_result_ = std::move(wrapped.result);
            research_status_ = research_result_.status;
            if (!research_result_.status.empty())
                aegis::AppendDiagnosticLine("research " + research_symbol_ + " " + research_result_.status);
        }

        void SignIn(bool manual)
        {
            SyncConfigFromBuffers();
            const std::string username = aegis::Trim(login_user_buffer_);
            const std::string password = aegis::Trim(login_password_buffer_);
            if (username.empty() || password.empty())
            {
                status_ = "Enter username and password.";
                return;
            }

            status_ = "Signing in...";
            const std::string login_url = aegis::JoinUrl(config_.auth_base_url, config_.login_path);
            const std::string body = "{\"identifier\":\"" + aegis::EscapeJson(username) + "\",\"password\":\"" + aegis::EscapeJson(password) + "\"}";
            aegis::HttpResponse response = aegis::HttpPostJson(login_url, body);
            aegis::JsonParseResult parsed = aegis::ParseJson(response.body);
            if (!parsed.ok || response.status_code >= 400)
            {
                const std::string form = "identifier=" + aegis::UrlEncode(username) + "&password=" + aegis::UrlEncode(password);
                response = aegis::HttpPostForm(login_url, form);
                parsed = aegis::ParseJson(response.body);
            }

            if (!response.error.empty())
            {
                status_ = "Could not reach auth bridge: " + response.error;
                aegis::AppendDiagnosticLine("login request failed: " + response.error);
                return;
            }
            if (response.status_code < 200 || response.status_code >= 300 || !parsed.ok || !parsed.value["ok"].AsBool(false))
            {
                status_ = "Login failed (" + std::to_string(response.status_code) + ").";
                aegis::AppendDiagnosticLine("login rejected status=" + std::to_string(response.status_code));
                authenticated_ = false;
                return;
            }

            cookie_header_ = aegis::CookieHeaderFromSetCookies(response.set_cookies);
            username_ = parsed.value["username"].AsString(username);
            authenticated_ = true;
            if (remember_me_ && config_.remember_credentials)
                aegis::SaveRememberedCredentials(username, password);
            else
                aegis::DeleteRememberedCredentials();
            status_ = manual ? "Signed in to website bridge." : "Remembered login accepted.";
            aegis::AppendDiagnosticLine("login accepted status=" + std::to_string(response.status_code));
        }

        void ExportJournal()
        {
            const std::filesystem::path path = aegis::AppDataDirectory() / "stock-journal.tsv";
            std::ofstream file(path, std::ios::app);
            if (!file)
            {
                status_ = "Could not write journal snapshot.";
                return;
            }
            const std::string seen_at = aegis::NowTimeLabel();
            for (const aegis::StockSignal& signal : state_.signals)
            {
                const aegis::StockQuote* quote = aegis::FindQuote(state_, signal.symbol);
                file << seen_at << '\t'
                     << "signal" << '\t'
                     << signal.symbol << '\t'
                     << signal.rating << '\t'
                     << signal.score << '\t'
                     << (quote ? aegis::FormatCurrency(quote->price) : "--") << '\t'
                     << aegis::FormatCurrency(signal.target_price) << '\t'
                     << aegis::FormatCurrency(signal.stop_level) << '\t'
                     << signal.risk << '\t'
                     << signal.thesis << '\n';
            }
            for (const aegis::TradePlan& plan : trade_plans_)
            {
                const double mark = PlanMarkPrice(plan);
                file << seen_at << '\t'
                     << "trade_plan" << '\t'
                     << plan.symbol << '\t'
                     << plan.rating << '\t'
                     << plan.status << '\t'
                     << aegis::FormatCurrency(plan.entry) << '\t'
                     << aegis::FormatCurrency(mark) << '\t'
                     << aegis::FormatCurrency(plan.target) << '\t'
                     << aegis::FormatCurrency(plan.stop) << '\t'
                     << plan.shares << " shares" << '\t'
                     << aegis::FormatCurrency(PlanUnrealizedPnl(plan)) << '\t'
                     << PlanProgressLabel(plan) << '\t'
                     << plan.thesis << '\n';
            }
            status_ = "Journal snapshot exported.";
            aegis::AppendDiagnosticLine("journal exported " + path.string());
        }

        double risk_min_ = 0.1;
        double risk_max_ = 25.0;
        std::string selected_symbol_for_worksheet_;
    };

    void ApplyTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsDark();
        style.WindowRounding = 0.0f;
        style.ChildRounding = 8.0f;
        style.FrameRounding = 7.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 8.0f;
        style.ScrollbarSize = 8.0f;
        style.GrabRounding = 8.0f;
        style.FrameBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.WindowBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(10.0f, 8.0f);
        style.FramePadding = ImVec2(11.0f, 8.0f);
        style.WindowPadding = ImVec2(12.0f, 12.0f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = V4(0.93f, 0.98f, 0.96f, 1.0f);
        colors[ImGuiCol_TextDisabled] = V4(0.42f, 0.49f, 0.48f, 1.0f);
        colors[ImGuiCol_WindowBg] = V4(0.012f, 0.020f, 0.022f, 1.0f);
        colors[ImGuiCol_ChildBg] = V4(0.025f, 0.040f, 0.040f, 0.96f);
        colors[ImGuiCol_PopupBg] = V4(0.025f, 0.040f, 0.040f, 0.99f);
        colors[ImGuiCol_Border] = V4(0.75f, 0.90f, 0.86f, 0.16f);
        colors[ImGuiCol_Separator] = V4(0.75f, 0.90f, 0.86f, 0.13f);
        colors[ImGuiCol_FrameBg] = V4(0.038f, 0.058f, 0.058f, 0.98f);
        colors[ImGuiCol_FrameBgHovered] = V4(0.060f, 0.120f, 0.105f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = V4(0.070f, 0.165f, 0.125f, 1.0f);
        colors[ImGuiCol_CheckMark] = V4(0.28f, 0.86f, 0.48f, 1.0f);
        colors[ImGuiCol_SliderGrab] = V4(0.28f, 0.86f, 0.48f, 1.0f);
        colors[ImGuiCol_Header] = V4(0.08f, 0.20f, 0.15f, 0.78f);
        colors[ImGuiCol_HeaderHovered] = V4(0.10f, 0.28f, 0.20f, 0.88f);
        colors[ImGuiCol_HeaderActive] = V4(0.12f, 0.34f, 0.23f, 1.0f);
        colors[ImGuiCol_TableHeaderBg] = V4(0.035f, 0.055f, 0.055f, 1.0f);
        colors[ImGuiCol_TableRowBg] = V4(0.025f, 0.040f, 0.040f, 0.48f);
        colors[ImGuiCol_TableRowBgAlt] = V4(0.040f, 0.060f, 0.060f, 0.54f);
        colors[ImGuiCol_Button] = V4(0.050f, 0.080f, 0.075f, 0.96f);
        colors[ImGuiCol_ButtonHovered] = V4(0.080f, 0.190f, 0.135f, 0.98f);
        colors[ImGuiCol_ButtonActive] = V4(0.105f, 0.300f, 0.185f, 1.0f);
        colors[ImGuiCol_PlotHistogram] = V4(0.28f, 0.86f, 0.48f, 1.0f);
        colors[ImGuiCol_PlotLines] = V4(0.55f, 0.88f, 1.0f, 1.0f);
        colors[ImGuiCol_TextSelectedBg] = V4(0.20f, 0.80f, 0.42f, 0.24f);
    }

    void ApplyLightTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsLight();
        style.WindowRounding = 0.0f;
        style.ChildRounding = 8.0f;
        style.FrameRounding = 7.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 8.0f;
        style.ScrollbarSize = 8.0f;
        style.GrabRounding = 8.0f;
        style.FrameBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.WindowBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(10.0f, 8.0f);
        style.FramePadding = ImVec2(11.0f, 8.0f);
        style.WindowPadding = ImVec2(12.0f, 12.0f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = V4(0.06f, 0.09f, 0.09f, 1.0f);
        colors[ImGuiCol_TextDisabled] = V4(0.42f, 0.48f, 0.47f, 1.0f);
        colors[ImGuiCol_WindowBg] = V4(0.94f, 0.96f, 0.95f, 1.0f);
        colors[ImGuiCol_ChildBg] = V4(0.985f, 0.99f, 0.985f, 0.98f);
        colors[ImGuiCol_PopupBg] = V4(0.99f, 1.0f, 0.99f, 0.99f);
        colors[ImGuiCol_Border] = V4(0.14f, 0.22f, 0.20f, 0.18f);
        colors[ImGuiCol_Separator] = V4(0.14f, 0.22f, 0.20f, 0.16f);
        colors[ImGuiCol_FrameBg] = V4(0.91f, 0.95f, 0.93f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = V4(0.82f, 0.91f, 0.86f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = V4(0.73f, 0.86f, 0.79f, 1.0f);
        colors[ImGuiCol_CheckMark] = V4(0.05f, 0.52f, 0.28f, 1.0f);
        colors[ImGuiCol_SliderGrab] = V4(0.05f, 0.52f, 0.28f, 1.0f);
        colors[ImGuiCol_Header] = V4(0.74f, 0.89f, 0.80f, 0.78f);
        colors[ImGuiCol_HeaderHovered] = V4(0.66f, 0.84f, 0.74f, 0.90f);
        colors[ImGuiCol_HeaderActive] = V4(0.56f, 0.76f, 0.65f, 1.0f);
        colors[ImGuiCol_TableHeaderBg] = V4(0.88f, 0.93f, 0.91f, 1.0f);
        colors[ImGuiCol_TableRowBg] = V4(0.97f, 0.99f, 0.98f, 0.62f);
        colors[ImGuiCol_TableRowBgAlt] = V4(0.91f, 0.96f, 0.94f, 0.72f);
        colors[ImGuiCol_Button] = V4(0.86f, 0.94f, 0.90f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = V4(0.72f, 0.88f, 0.80f, 1.0f);
        colors[ImGuiCol_ButtonActive] = V4(0.58f, 0.78f, 0.68f, 1.0f);
        colors[ImGuiCol_PlotHistogram] = V4(0.05f, 0.58f, 0.30f, 1.0f);
        colors[ImGuiCol_PlotLines] = V4(0.05f, 0.38f, 0.70f, 1.0f);
        colors[ImGuiCol_TextSelectedBg] = V4(0.10f, 0.52f, 0.28f, 0.25f);
    }

    void ApplyHighContrastTheme(bool light_theme)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameBorderSize = 1.5f;
        style.ChildBorderSize = 1.5f;
        style.PopupBorderSize = 1.5f;
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 14.0f;
        style.FramePadding = ImVec2(12.0f, 9.0f);

        ImVec4* colors = style.Colors;
        if (light_theme)
        {
            colors[ImGuiCol_Text] = V4(0.0f, 0.0f, 0.0f, 1.0f);
            colors[ImGuiCol_TextDisabled] = V4(0.22f, 0.22f, 0.22f, 1.0f);
            colors[ImGuiCol_WindowBg] = V4(1.0f, 1.0f, 1.0f, 1.0f);
            colors[ImGuiCol_ChildBg] = V4(0.98f, 0.99f, 1.0f, 1.0f);
            colors[ImGuiCol_PopupBg] = V4(1.0f, 1.0f, 1.0f, 1.0f);
            colors[ImGuiCol_Border] = V4(0.0f, 0.0f, 0.0f, 0.52f);
            colors[ImGuiCol_Separator] = V4(0.0f, 0.0f, 0.0f, 0.46f);
            colors[ImGuiCol_FrameBg] = V4(0.92f, 0.96f, 1.0f, 1.0f);
            colors[ImGuiCol_FrameBgHovered] = V4(0.78f, 0.88f, 1.0f, 1.0f);
            colors[ImGuiCol_FrameBgActive] = V4(0.64f, 0.78f, 0.96f, 1.0f);
            colors[ImGuiCol_Header] = V4(0.78f, 0.88f, 1.0f, 1.0f);
            colors[ImGuiCol_HeaderHovered] = V4(0.58f, 0.76f, 1.0f, 1.0f);
            colors[ImGuiCol_HeaderActive] = V4(0.34f, 0.58f, 0.98f, 1.0f);
            colors[ImGuiCol_Button] = V4(0.84f, 0.92f, 1.0f, 1.0f);
            colors[ImGuiCol_ButtonHovered] = V4(0.62f, 0.78f, 1.0f, 1.0f);
            colors[ImGuiCol_ButtonActive] = V4(0.38f, 0.62f, 1.0f, 1.0f);
            colors[ImGuiCol_CheckMark] = V4(0.0f, 0.28f, 0.86f, 1.0f);
            colors[ImGuiCol_SliderGrab] = V4(0.0f, 0.28f, 0.86f, 1.0f);
            colors[ImGuiCol_TableHeaderBg] = V4(0.86f, 0.91f, 1.0f, 1.0f);
            colors[ImGuiCol_TableRowBg] = V4(1.0f, 1.0f, 1.0f, 1.0f);
            colors[ImGuiCol_TableRowBgAlt] = V4(0.92f, 0.96f, 1.0f, 1.0f);
            colors[ImGuiCol_TextSelectedBg] = V4(0.20f, 0.52f, 1.0f, 0.32f);
        }
        else
        {
            colors[ImGuiCol_Text] = V4(1.0f, 1.0f, 1.0f, 1.0f);
            colors[ImGuiCol_TextDisabled] = V4(0.72f, 0.74f, 0.78f, 1.0f);
            colors[ImGuiCol_WindowBg] = V4(0.0f, 0.0f, 0.0f, 1.0f);
            colors[ImGuiCol_ChildBg] = V4(0.02f, 0.025f, 0.03f, 1.0f);
            colors[ImGuiCol_PopupBg] = V4(0.02f, 0.025f, 0.03f, 1.0f);
            colors[ImGuiCol_Border] = V4(1.0f, 1.0f, 1.0f, 0.40f);
            colors[ImGuiCol_Separator] = V4(1.0f, 1.0f, 1.0f, 0.34f);
            colors[ImGuiCol_FrameBg] = V4(0.08f, 0.10f, 0.14f, 1.0f);
            colors[ImGuiCol_FrameBgHovered] = V4(0.12f, 0.18f, 0.24f, 1.0f);
            colors[ImGuiCol_FrameBgActive] = V4(0.18f, 0.26f, 0.35f, 1.0f);
            colors[ImGuiCol_Header] = V4(0.08f, 0.28f, 0.46f, 1.0f);
            colors[ImGuiCol_HeaderHovered] = V4(0.10f, 0.38f, 0.62f, 1.0f);
            colors[ImGuiCol_HeaderActive] = V4(0.14f, 0.48f, 0.78f, 1.0f);
            colors[ImGuiCol_Button] = V4(0.08f, 0.22f, 0.34f, 1.0f);
            colors[ImGuiCol_ButtonHovered] = V4(0.10f, 0.34f, 0.54f, 1.0f);
            colors[ImGuiCol_ButtonActive] = V4(0.16f, 0.44f, 0.70f, 1.0f);
            colors[ImGuiCol_CheckMark] = V4(1.0f, 0.86f, 0.20f, 1.0f);
            colors[ImGuiCol_SliderGrab] = V4(1.0f, 0.86f, 0.20f, 1.0f);
            colors[ImGuiCol_TableHeaderBg] = V4(0.08f, 0.12f, 0.18f, 1.0f);
            colors[ImGuiCol_TableRowBg] = V4(0.02f, 0.025f, 0.03f, 1.0f);
            colors[ImGuiCol_TableRowBgAlt] = V4(0.06f, 0.07f, 0.09f, 1.0f);
            colors[ImGuiCol_TextSelectedBg] = V4(0.10f, 0.42f, 0.82f, 0.45f);
        }
    }

    HICON CreateAegisWindowIcon(int size)
    {
        HDC screen = GetDC(nullptr);
        HDC dc = CreateCompatibleDC(screen);
        HBITMAP color = CreateCompatibleBitmap(screen, size, size);
        HBITMAP old_color = static_cast<HBITMAP>(SelectObject(dc, color));

        RECT rect{ 0, 0, size, size };
        HBRUSH bg = CreateSolidBrush(RGB(3, 10, 10));
        FillRect(dc, &rect, bg);
        DeleteObject(bg);

        HPEN glow = CreatePen(PS_SOLID, std::max(1, size / 12), RGB(66, 235, 130));
        HPEN old_pen = static_cast<HPEN>(SelectObject(dc, glow));
        HBRUSH hollow = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(dc, hollow));

        POINT shield[6] = {
            { size / 2, size / 7 },
            { size * 6 / 7, size * 2 / 7 },
            { size * 5 / 6, size * 2 / 3 },
            { size / 2, size * 6 / 7 },
            { size / 6, size * 2 / 3 },
            { size / 7, size * 2 / 7 }
        };
        Polygon(dc, shield, 6);

        HFONT font = CreateFontW(-static_cast<int>(size * 0.55f), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
        HFONT old_font = static_cast<HFONT>(SelectObject(dc, font));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(100, 255, 160));
        DrawTextW(dc, L"S", 1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(dc, old_font);
        DeleteObject(font);
        SelectObject(dc, old_brush);
        SelectObject(dc, old_pen);
        DeleteObject(glow);
        SelectObject(dc, old_color);

        HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
        ICONINFO info{};
        info.fIcon = TRUE;
        info.hbmColor = color;
        info.hbmMask = mask;
        HICON icon = CreateIconIndirect(&info);

        DeleteObject(mask);
        DeleteObject(color);
        DeleteDC(dc);
        ReleaseDC(nullptr, screen);
        return icon;
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv != nullptr)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = aegis::Lower(aegis::WideToUtf8(argv[i]));
            if (arg == "--self-test" || arg == "/self-test")
            {
                LocalFree(argv);
                return aegis::RunSelfTests();
            }
        }
        LocalFree(argv);
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HICON app_icon = CreateAegisWindowIcon(32);
    HICON app_icon_small = CreateAegisWindowIcon(16);
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, app_icon, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"AegisStockBettingAI", app_icon_small };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Aegis Stock Investing AI", WS_OVERLAPPEDWINDOW, 100, 100, 1560, 930, nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd != nullptr)
    {
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    }

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        if (app_icon)
            DestroyIcon(app_icon);
        if (app_icon_small)
            DestroyIcon(app_icon_small);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    const char* segoe = "C:\\Windows\\Fonts\\segoeui.ttf";
    const char* segoe_bold = "C:\\Windows\\Fonts\\segoeuib.ttf";
    g_font_regular = io.Fonts->AddFontFromFileTTF(segoe, 16.0f);
    g_font_bold = io.Fonts->AddFontFromFileTTF(segoe_bold, 16.0f);
    g_font_title = io.Fonts->AddFontFromFileTTF(segoe_bold, 24.0f);
    if (!g_font_regular)
        g_font_regular = io.Fonts->AddFontDefault();
    if (!g_font_bold)
        g_font_bold = g_font_regular;
    if (!g_font_title)
        g_font_title = g_font_regular;

    ApplyTheme();

    StockApp app;
    app.Initialize();

    ImVec4 clear_color = V4(0.018f, 0.032f, 0.032f, 1.0f);
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        app.Render();
        ImGui::Render();

        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        const HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    app.Shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    if (app_icon)
        DestroyIcon(app_icon);
    if (app_icon_small)
        DestroyIcon(app_icon_small);
    return 0;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;
    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = kMinWindowWidth;
        info->ptMinTrackSize.y = kMinWindowHeight;
        return 0;
    }
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = static_cast<UINT>(LOWORD(lParam));
        g_ResizeHeight = static_cast<UINT>(HIWORD(lParam));
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
}

