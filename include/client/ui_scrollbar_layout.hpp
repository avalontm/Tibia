#pragma once

#include <cstddef>

namespace tibia::client {

struct UiScrollbarRect {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

struct UiScrollbarGeometry {
    bool thumb_visible{};
    UiScrollbarRect up_arrow;
    UiScrollbarRect track;
    UiScrollbarRect thumb;
    UiScrollbarRect down_arrow;
};

[[nodiscard]] UiScrollbarGeometry ui_scrollbar_geometry(int viewport_width,
    int viewport_height, std::size_t item_count, int visible_items,
    int first_visible, int margin, int scrollbar_width, int arrow_height,
    int thumb_cap_height);
[[nodiscard]] int ui_scrollbar_scroll_after_track_click(int pointer_y,
    const UiScrollbarGeometry& geometry, int first_visible,
    std::size_t item_count, int visible_items);
[[nodiscard]] int ui_scrollbar_scroll_after_thumb_drag(int pointer_y,
    int grab_offset, const UiScrollbarGeometry& geometry,
    std::size_t item_count, int visible_items);

}
