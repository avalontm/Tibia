#pragma once

#include "client/content_window_stack_layout.hpp"

#include <array>
#include <cstdint>

namespace tibia::client {

struct ContentWindowFramePart {
    std::uint16_t skin{};
    ContentWindowRect rect;
    bool tiled{};
};

struct ContentWindowFrameLayout {
    std::array<ContentWindowFramePart, 8> parts;
    ContentWindowRect title_bar;
    ContentWindowRect body;
};

inline constexpr int content_window_frame_side_width = 4;
inline constexpr int content_window_frame_top_height = 15;
inline constexpr int content_window_frame_bottom_height = 4;
inline constexpr int content_window_frame_header_height =
    content_window_frame_top_height + content_window_frame_bottom_height;

[[nodiscard]] ContentWindowFrameLayout layout_content_window_frame(
    const ContentWindowRect& window);

}
