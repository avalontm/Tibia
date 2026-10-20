#pragma once

#include "client/communication_display_preferences.hpp"
#include "client/communication_router.hpp"
#include "client/content_window_state.hpp"
#include "client/hotkey_preferences.hpp"
#include "client/skills_panel_model.hpp"

#include <cstdint>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace tibia::client {

struct BuddyPreference {
    std::uint32_t id{};
    std::int32_t type{};
    std::string name;
    bool enabled{};
};

struct ClientCommunicationPreferences {
    std::int32_t console_height{};
    std::int32_t basic_brightness{32};
    std::int32_t confine_framerate{};
    std::int32_t engine{-1};
    std::int32_t automap_zoom{2};
    std::array<std::int32_t, 3> fullscreen_mode{};
    std::array<std::int32_t, 3> buddy_sort_methods{};
    bool fullscreen_mode_configured{};
    bool buddy_hide_offline{};
    bool light_effects{true};
    bool no_stretching{};
    bool windows_mouse_pointer{true};
    bool test_server_login{};
    bool classic_control{};
    bool auto_chase_off{true};
    std::int32_t attack_mode{2};
    std::int32_t chase_mode{};
    bool secure_mode{true};
    bool show_new_hints{true};
    SkillsPanelBarVisibility skills_bar_visibility;
    std::int32_t creature_info{2};
    bool textual_effects{true};
    CommunicationRoutingSettings routing;
    CommunicationDisplayPreferences display;
    std::array<HotkeyPreference, hotkey_count> hotkeys{};
    std::vector<std::int32_t> open_dialogs;
    std::vector<ContentWindowPreference> content_windows;
    std::vector<std::int32_t> known_hints;
    std::vector<BuddyPreference> buddies;
    std::vector<std::string> white_list;
    std::vector<std::string> ignore_list;
};

[[nodiscard]] ClientCommunicationPreferences read_communication_preferences(
    const std::filesystem::path& path);
void write_communication_preferences(const std::filesystem::path& path,
    const ClientCommunicationPreferences& preferences);

}
