#include "client/quest_log_layout.hpp"

#include <algorithm>

namespace tibia::client {

int quest_log_visible_rows(int viewport_height, int row_height, int content_margin) {
    if (row_height <= 0) return 0;
    return std::max(0, (viewport_height - 2 * content_margin) / row_height);
}

int quest_log_max_scroll(std::size_t entry_count, int viewport_height,
    int row_height, int content_margin) {
    return std::max(0, static_cast<int>(entry_count) -
        quest_log_visible_rows(viewport_height, row_height, content_margin));
}

int quest_log_scroll_for_selection(int selected, int first_visible,
    std::size_t entry_count, int viewport_height, int row_height, int content_margin) {
    const int visible_rows = quest_log_visible_rows(viewport_height, row_height,
        content_margin);
    if (selected < first_visible) first_visible = selected;
    else if (selected >= first_visible + visible_rows)
        first_visible = selected - visible_rows + 1;
    return std::clamp(first_visible, 0,
        quest_log_max_scroll(entry_count, viewport_height, row_height, content_margin));
}

QuestLogScrollbarGeometry quest_log_scrollbar_geometry(int list_width, int list_height,
    std::size_t entry_count, int first_visible, int row_height, int content_margin,
    int scrollbar_width, int arrow_height, int thumb_cap_height) {
    QuestLogScrollbarGeometry geometry;
    const int visible_rows = quest_log_visible_rows(list_height, row_height, content_margin);
    const int maximum = quest_log_max_scroll(entry_count, list_height,
        row_height, content_margin);
    if (scrollbar_width <= 0 || arrow_height <= 0) return geometry;

    const int x = list_width - content_margin - scrollbar_width;
    const int track_height = std::max(0,
        list_height - 2 * content_margin - 2 * arrow_height);
    geometry.up_arrow = {x, content_margin, x + scrollbar_width,
        content_margin + arrow_height};
    geometry.track = {x, content_margin + arrow_height, x + scrollbar_width,
        list_height - content_margin - arrow_height};
    geometry.down_arrow = {x, list_height - content_margin - arrow_height,
        x + scrollbar_width, list_height - content_margin};
    if (maximum == 0 || entry_count == 0 || track_height <= 0) return geometry;

    const int thumb_minimum = 2 * thumb_cap_height;
    const int thumb_height = std::clamp(
        track_height * visible_rows / static_cast<int>(entry_count),
        std::min(thumb_minimum, track_height), track_height);
    const int offset = (std::clamp(first_visible, 0, maximum) *
        (track_height - thumb_height)) / maximum;
    const int thumb_top = content_margin + arrow_height + offset;

    geometry.visible = true;
    geometry.thumb = {x, thumb_top, x + scrollbar_width, thumb_top + thumb_height};
    return geometry;
}

int quest_log_scroll_after_track_click(int pointer_y,
    const QuestLogScrollbarGeometry& geometry, int first_visible,
    std::size_t entry_count, int viewport_height, int row_height, int content_margin) {
    const int page = quest_log_visible_rows(viewport_height, row_height, content_margin);
    if (pointer_y < geometry.thumb.top) first_visible -= std::max(0, page - 1);
    else if (pointer_y >= geometry.thumb.bottom)
        first_visible += std::max(0, page - 1);
    return std::clamp(first_visible, 0,
        quest_log_max_scroll(entry_count, viewport_height, row_height, content_margin));
}

int quest_log_scroll_after_thumb_drag(int pointer_y, int grab_offset,
    const QuestLogScrollbarGeometry& geometry, int first_visible,
    std::size_t entry_count, int viewport_height, int row_height, int content_margin) {
    const int maximum = quest_log_max_scroll(entry_count, viewport_height,
        row_height, content_margin);
    const int thumb_height = geometry.thumb.bottom - geometry.thumb.top;
    const int travel = geometry.track.bottom - geometry.track.top - thumb_height;
    if (maximum <= 0 || travel < 0) return 0;
    const int relative = std::clamp(pointer_y - grab_offset - geometry.track.top,
        0, travel);
    return std::clamp(relative * (maximum + 1) / (travel + 1), 0, maximum);
}

}
