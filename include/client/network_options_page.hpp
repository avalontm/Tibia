#pragma once

#include <array>
#include <string_view>

namespace tibia::client {

inline constexpr int network_options_content_width = 200;
inline constexpr int network_options_content_height = 22;
inline constexpr int network_options_dialog_width = 236;
inline constexpr int network_options_dialog_height = 114;
inline constexpr int network_options_content_x = 18;
inline constexpr int network_options_content_y = 32;
inline constexpr int network_options_row_height = 22;
inline constexpr std::array<int, 1> network_options_row_y{0};
inline constexpr std::array<std::wstring_view, 1> network_options_labels{
    L"Test Server Login"};

}
