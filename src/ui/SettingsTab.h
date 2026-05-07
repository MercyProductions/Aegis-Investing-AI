#pragma once

#include "StockData.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace auralith::ui
{
    struct SettingsTabContext
    {
        aegis::Config& config;
        char* api_key_buffer = nullptr;
        std::size_t api_key_buffer_size = 0;
        char* watchlist_buffer = nullptr;
        std::size_t watchlist_buffer_size = 0;
        char* sec_user_agent_buffer = nullptr;
        std::size_t sec_user_agent_buffer_size = 0;
        char* website_base_buffer = nullptr;
        std::size_t website_base_buffer_size = 0;
        char* auth_base_buffer = nullptr;
        std::size_t auth_base_buffer_size = 0;
        char* login_user_buffer = nullptr;
        std::size_t login_user_buffer_size = 0;
        char* login_password_buffer = nullptr;
        std::size_t login_password_buffer_size = 0;

        bool& remember_me;
        bool& compact_mode;
        bool& authenticated;
        bool validation_in_flight = false;
        std::string& validation_status;

        std::function<void(const char*, const char*)> section_title;
        std::function<bool(const char*)> small_action_button;
        std::function<void(const char*)> text_muted;
        std::function<void(const char*, const std::vector<aegis::InfoItem>&)> render_info_table;
        std::function<std::vector<aegis::InfoItem>()> build_shortcut_help_rows;
        std::function<std::vector<aegis::InfoItem>()> build_background_task_rows;

        std::function<void()> save_settings;
        std::function<void()> begin_validation;
        std::function<void()> sign_in;
        std::function<void()> clear_remembered;
        std::function<void()> clear_all_secrets;
        std::function<void(const std::string&)> persist_ui_preferences;
        std::function<void()> open_diagnostics_folder;
        std::function<void()> run_self_test;
    };

    void RenderSettingsTab(SettingsTabContext& ctx);
}
