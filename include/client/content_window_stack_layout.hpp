#pragma once

#include <span>
#include <vector>

namespace tibia::client {

struct ContentWindowRect {
    int x{};
    int y{};
    int width{};
    int height{};
};

inline constexpr int content_window_stack_width = 176;

[[nodiscard]] std::vector<ContentWindowRect> layout_docked_content_windows(
    int origin_x,
    int origin_y,
    int width,
    std::span<const int> heights);

[[nodiscard]] std::vector<ContentWindowRect> layout_main_content_window_stack(
    int client_width,
    std::span<const int> heights,
    int origin_y = 0);

}
