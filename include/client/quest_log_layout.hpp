#pragma once

#include <cstddef>

namespace tibia::client {

inline constexpr int quest_log_row_height = 16;
inline constexpr int quest_log_content_margin = 3;
inline constexpr int quest_scrollbar_width = 12;
inline constexpr int quest_scrollbar_arrow_height = 12;
inline constexpr int quest_scrollbar_thumb_cap_height = 6;

struct QuestLogRect {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

struct QuestLogScrollbarGeometry {
    bool visible{};
    QuestLogRect up_arrow;
    QuestLogRect track;
    QuestLogRect thumb;
    QuestLogRect down_arrow;
};

[[nodiscard]] int quest_log_visible_rows(int viewport_height, int row_height,
    int content_margin);
[[nodiscard]] int quest_log_max_scroll(std::size_t entry_count, int viewport_height,
    int row_height, int content_margin);
[[nodiscard]] int quest_log_scroll_for_selection(int selected, int first_visible,
    std::size_t entry_count, int viewport_height, int row_height, int content_margin);
[[nodiscard]] QuestLogScrollbarGeometry quest_log_scrollbar_geometry(int list_width,
    int list_height, std::size_t entry_count, int first_visible, int row_height,
    int content_margin, int scrollbar_width, int arrow_height, int thumb_cap_height);
[[nodiscard]] int quest_log_scroll_after_track_click(int pointer_y,
    const QuestLogScrollbarGeometry& geometry, int first_visible,
    std::size_t entry_count, int viewport_height, int row_height, int content_margin);
[[nodiscard]] int quest_log_scroll_after_thumb_drag(int pointer_y, int grab_offset,
    const QuestLogScrollbarGeometry& geometry, int first_visible,
    std::size_t entry_count, int viewport_height, int row_height, int content_margin);

}
