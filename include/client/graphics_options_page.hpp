#pragma once

#include <string>

namespace tibia::client {

inline constexpr int graphics_options_content_width = 200;
inline constexpr int graphics_options_content_height = 185;
inline constexpr int graphics_options_dialog_width = 236;
inline constexpr int graphics_options_dialog_height = 277;
inline constexpr int graphics_options_content_x = 18;
inline constexpr int graphics_options_content_y = 32;
inline constexpr int graphics_options_fullscreen_y = 0;
inline constexpr int graphics_options_label_y = 35;
inline constexpr int graphics_options_list_y = 50;
inline constexpr int graphics_options_list_height = 104;
inline constexpr int graphics_options_advanced_y = 167;
inline constexpr int graphics_options_footer_y = 237;
inline constexpr int graphics_options_button_y = 247;
inline constexpr int graphics_options_button_x = 114;
inline constexpr int graphics_options_button_width = 86;
inline constexpr int graphics_options_row_height = 16;
inline constexpr int graphics_options_visible_rows = 6;

struct GraphicsDisplayMode {
    int width{};
    int height{};
    int bits_per_pixel{};
    int refresh_rate{};
    std::wstring label;
};

}
