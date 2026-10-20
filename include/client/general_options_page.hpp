#pragma once

#include <array>
#include <string_view>

namespace tibia::client {

inline constexpr int general_options_content_width = 200;
inline constexpr int general_options_content_height = 154;
inline constexpr int general_options_dialog_width = 236;
inline constexpr int general_options_dialog_height = 246;
inline constexpr int general_options_content_x = 18;
inline constexpr int general_options_content_y = 32;
inline constexpr int general_options_row_height = 22;
inline constexpr std::array<int, 5> general_options_row_y{0, 33, 66, 99, 132};
inline constexpr std::array<std::wstring_view, 5> general_options_labels{
    L"Tibia Classic Control",
    L"Auto Chase Off",
    L"Show Hints",
    L"Show Names of Creatures",
    L"Show Textual Effects"};

}
