#pragma once

#include "ui/AuralithWidgets.h"

#include <cstddef>
#include <string>
#include <vector>

namespace auralith::ui
{
    struct PageHeaderSpec
    {
        std::string title;
        std::string subtitle;
        std::vector<StatusPillSpec> badges;
        std::string primary_action;
        Tone action_tone = Tone::Info;
    };

    struct CommandBarSpec
    {
        char* command_buffer = nullptr;
        std::size_t command_buffer_size = 0;
        char* search_buffer = nullptr;
        std::size_t search_buffer_size = 0;
        bool focus_command = false;
        bool refreshing = false;
        int unread_alerts = 0;
        std::string market_state;
        std::string provider_state;
        std::string shortcut_hint = "Ctrl+K";
    };

    struct CommandBarResult
    {
        bool command_submitted = false;
        bool refresh_clicked = false;
        bool alerts_clicked = false;
        bool open_web_clicked = false;
        bool theme_clicked = false;
    };

    bool PageHeader(const PageHeaderSpec& spec);
    CommandBarResult CommandBar(const CommandBarSpec& spec);

    bool BeginPageSection(const char* id, const char* title, const char* subtitle = nullptr, ImVec2 size = ImVec2(0, 0), bool elevated = false);
    void EndPageSection();

    void BeginTwoColumn(float left_width, float gap = 12.0f);
    void NextColumn();
    void EndTwoColumn();
}
