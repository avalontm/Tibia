#pragma once

namespace tibia::client {

struct SkillsPanelScrollRange {
    int minimum{};
    int maximum{};
    int value{};
    int page{};
    int line_step{};
};

struct SkillsPanelScrollRect {
    int x{};
    int y{};
    int width{};
    int height{};
};

struct SkillsPanelScrollbarGeometry {
    bool thumb_visible{};
    SkillsPanelScrollRect up_arrow;
    SkillsPanelScrollRect track;
    SkillsPanelScrollRect thumb;
    SkillsPanelScrollRect down_arrow;
};

inline constexpr int skills_panel_scroll_arrow_height = 12;
inline constexpr int skills_panel_scroll_thumb_cap_height = 6;

[[nodiscard]] SkillsPanelScrollRange make_skills_panel_scroll_range(
    int natural_content_height,
    int viewport_height,
    int current_value,
    int font_height);

[[nodiscard]] SkillsPanelScrollbarGeometry layout_skills_panel_scrollbar(
    const SkillsPanelScrollRect& bounds,
    const SkillsPanelScrollRange& range);

[[nodiscard]] int skills_panel_scroll_by_lines(
    const SkillsPanelScrollRange& range,
    int direction,
    int line_count = 1);
[[nodiscard]] int skills_panel_scroll_by_pages(
    const SkillsPanelScrollRange& range,
    int direction,
    int page_count = 1);
[[nodiscard]] int skills_panel_scroll_after_track_click(
    const SkillsPanelScrollRange& range,
    const SkillsPanelScrollbarGeometry& geometry,
    int pointer_y);
[[nodiscard]] int skills_panel_scroll_after_thumb_drag(
    const SkillsPanelScrollRange& range,
    int track_height,
    int thumb_height,
    int grab_offset,
    int pointer_y);

}
