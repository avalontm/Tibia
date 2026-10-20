#pragma once

#include "client/hotkey_preferences.hpp"

#include <array>
#include <cstddef>
#include <string>

namespace tibia::client {

inline constexpr int hotkey_options_content_width = 251;
inline constexpr int hotkey_options_content_height = 313;
inline constexpr int hotkey_options_dialog_width = 287;
inline constexpr int hotkey_options_dialog_height = 405;
inline constexpr int hotkey_options_content_x = 18;
inline constexpr int hotkey_options_content_y = 32;
inline constexpr int hotkey_list_y = 15;
inline constexpr int hotkey_list_height = 104;
inline constexpr int hotkey_list_row_height = 10;
inline constexpr std::size_t hotkey_list_visible_rows = 10;
inline constexpr int hotkey_text_y = 147;
inline constexpr int hotkey_text_height = 16;
inline constexpr int hotkey_automatic_y = 176;
inline constexpr int hotkey_automatic_height = 22;
inline constexpr int hotkey_object_label_y = 211;
inline constexpr int hotkey_object_label_height = 14;
inline constexpr int hotkey_object_x = 0;
inline constexpr int hotkey_object_y = 226;
inline constexpr int hotkey_object_size = 53;
inline constexpr int hotkey_select_x = 66;
inline constexpr int hotkey_select_y = 226;
inline constexpr int hotkey_clear_x = 66;
inline constexpr int hotkey_clear_y = 259;
inline constexpr int hotkey_mode_x = 165;
inline constexpr std::array<int, 3> hotkey_mode_y{226, 259, 293};
inline constexpr int hotkey_button_width = 86;
inline constexpr int hotkey_button_height = 20;
inline constexpr int hotkey_object_selection_mode = 8;
inline constexpr int hotkey_footer_y = 375;
inline constexpr std::array<int, 2> hotkey_footer_x{178, 231};
inline constexpr std::array<const char*, 3> hotkey_mode_labels{
    "Use on yourself", "Use on target", "With crosshairs"};

[[nodiscard]] std::string format_hotkey_entry(
    std::size_t index, const HotkeyPreference& preference);
[[nodiscard]] std::string hotkey_object_action(std::int32_t mode);
void clear_hotkey(HotkeyPreference& preference) noexcept;
void set_hotkey_object_mode(HotkeyPreference& preference, std::int32_t mode) noexcept;

}
