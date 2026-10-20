#include "client/content_window_stack_layout.hpp"

namespace tibia::client {

std::vector<ContentWindowRect> layout_docked_content_windows(
    int origin_x,
    int origin_y,
    int width,
    std::span<const int> heights) {
    std::vector<ContentWindowRect> result;
    result.reserve(heights.size());
    auto y = origin_y;
    for (const auto height : heights) {
        result.push_back({origin_x, y, width, height});
        y += height;
    }
    return result;
}

std::vector<ContentWindowRect> layout_main_content_window_stack(
    int client_width,
    std::span<const int> heights,
    int origin_y) {
    return layout_docked_content_windows(
        client_width - content_window_stack_width, origin_y,
        content_window_stack_width, heights);
}

}
