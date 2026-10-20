#include "client/content_window_frame_layout.hpp"

#include <algorithm>

namespace tibia::client {

ContentWindowFrameLayout layout_content_window_frame(
    const ContentWindowRect& window) {
    const auto side = std::min(content_window_frame_side_width,
        std::max(0, window.width / 2));
    const auto top = std::min(content_window_frame_top_height,
        std::max(0, window.height));
    const auto bottom = std::min(content_window_frame_bottom_height,
        std::max(0, window.height - top));
    const auto center_width = std::max(0, window.width - side * 2);
    const auto center_height = std::max(0, window.height - top - bottom);
    const auto right_x = window.x + window.width - side;
    const auto bottom_y = window.y + window.height - bottom;

    ContentWindowFrameLayout layout;
    layout.parts = {{
        {128, {window.x, window.y, side, top}, false},
        {129, {right_x, window.y, side, top}, false},
        {130, {window.x, bottom_y, side, bottom}, false},
        {131, {right_x, bottom_y, side, bottom}, false},
        {132, {window.x + side, window.y, center_width, top}, true},
        {133, {window.x + side, bottom_y, center_width, bottom}, true},
        {134, {window.x, window.y + top, side, center_height}, true},
        {135, {right_x, window.y + top, side, center_height}, true}
    }};
    layout.title_bar = {window.x + side, window.y, center_width, top};
    layout.body = {window.x + side, window.y + top, center_width, center_height};
    return layout;
}

}
