#pragma once

#include <array>
#include <string_view>

namespace tibia::client {

inline constexpr int console_options_content_width = 260;
inline constexpr int console_options_content_height = 197;
inline constexpr int console_options_dialog_width = 296;
inline constexpr int console_options_dialog_height = 289;
inline constexpr int console_options_content_x = 18;
inline constexpr int console_options_content_y = 32;
inline constexpr int console_options_row_height = 22;
inline constexpr std::array<int, 6> console_options_row_y{0, 35, 70, 105, 140, 175};
inline constexpr std::array<std::wstring_view, 6> console_options_labels{
    L"Show Info Messages in Console",
    L"Show Event Messages in Console",
    L"Show Status Messages in Console",
    L"Show Timestamps in Console",
    L"Show Levels in Console",
    L"Show Private Messages in Game Window"};

}
