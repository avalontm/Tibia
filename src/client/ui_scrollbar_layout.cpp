#include "client/ui_scrollbar_layout.hpp"

#include <algorithm>
#include <cmath>

namespace tibia::client {

namespace {

int max_scroll(std::size_t item_count, int visible_items) {
    return std::max(0, static_cast<int>(item_count) - visible_items);
}

}

UiScrollbarGeometry ui_scrollbar_geometry(int viewport_width, int viewport_height,
    std::size_t item_count, int visible_items, int first_visible, int margin,
    int scrollbar_width, int arrow_height, int thumb_cap_height) {
    UiScrollbarGeometry geometry;
    if (viewport_width <= 0 || viewport_height <= 0 || margin < 0 ||
        scrollbar_width <= 0 || arrow_height <= 0 || thumb_cap_height <= 0)
        return geometry;

    const int x = viewport_width - margin - scrollbar_width;
    const int track_height = std::max(0,
        viewport_height - 2 * margin - 2 * arrow_height);
    geometry.up_arrow = {x, margin, x + scrollbar_width, margin + arrow_height};
    geometry.track = {x, margin + arrow_height, x + scrollbar_width,
        viewport_height - margin - arrow_height};
    geometry.down_arrow = {x, viewport_height - margin - arrow_height,
        x + scrollbar_width, viewport_height - margin};

    const int maximum = max_scroll(item_count, visible_items);
    if (maximum == 0 || item_count == 0 || visible_items <= 0 || track_height <= 0)
        return geometry;

    const int minimum_thumb = std::min(2 * thumb_cap_height, track_height);
    const float visible_ratio = static_cast<float>(visible_items) /
        static_cast<float>(item_count);
    const int proportional_thumb = static_cast<int>(std::nearbyint(
        static_cast<double>(visible_ratio) * track_height));
    const int thumb_height = std::clamp(proportional_thumb, minimum_thumb, track_height);
    const int travel = track_height - thumb_height;
    const int position = std::clamp(first_visible, 0, maximum) * travel / maximum;
    const int top = margin + arrow_height + position;
    geometry.thumb_visible = true;
    geometry.thumb = {x, top, x + scrollbar_width, top + thumb_height};
    return geometry;
}

int ui_scrollbar_scroll_after_track_click(int pointer_y,
    const UiScrollbarGeometry& geometry, int first_visible,
    std::size_t item_count, int visible_items) {
    const int page = std::max(0, visible_items - 1);
    if (pointer_y < geometry.thumb.top) first_visible -= page;
    else if (pointer_y >= geometry.thumb.bottom) first_visible += page;
    return std::clamp(first_visible, 0, max_scroll(item_count, visible_items));
}

int ui_scrollbar_scroll_after_release(int pointer_x, int pointer_y,
    int pressed_part, const UiScrollbarGeometry& geometry, int first_visible,
    std::size_t item_count, int visible_items) {
    const auto contains = [pointer_x, pointer_y](const UiScrollbarRect& rect) {
        return pointer_x >= rect.left && pointer_y >= rect.top &&
            pointer_x < rect.right && pointer_y < rect.bottom;
    };
    if (pressed_part == 0 && contains(geometry.up_arrow)) {
        --first_visible;
    } else if (pressed_part == 1 && contains(geometry.down_arrow)) {
        ++first_visible;
    } else if (pressed_part == 2 && geometry.thumb_visible &&
        contains(geometry.track)) {
        first_visible = ui_scrollbar_scroll_after_track_click(pointer_y,
            geometry, first_visible, item_count, visible_items);
    }
    return std::clamp(first_visible, 0, max_scroll(item_count, visible_items));
}

int ui_scrollbar_scroll_after_thumb_drag(int pointer_y, int grab_offset,
    const UiScrollbarGeometry& geometry, std::size_t item_count,
    int visible_items) {
    const int maximum = max_scroll(item_count, visible_items);
    const int thumb_height = geometry.thumb.bottom - geometry.thumb.top;
    const int track_height = geometry.track.bottom - geometry.track.top;
    const int travel = track_height - thumb_height;
    if (maximum <= 0 || travel <= 0) return 0;
    const int relative = std::clamp(pointer_y - grab_offset - geometry.track.top,
        0, travel + 1);
    return std::clamp(static_cast<int>(static_cast<long long>(relative) *
        (static_cast<long long>(maximum) + 1) /
        (static_cast<long long>(travel) + 1)), 0, maximum);
}

}
