#include "client/skills_panel_scroll.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tibia::client {

SkillsPanelScrollRange make_skills_panel_scroll_range(
    int natural_content_height,
    int viewport_height,
    int current_value,
    int font_height) {
    const auto page = std::max(0, viewport_height);
    const auto content = std::max(natural_content_height, page);
    const auto maximum = content - page;
    return {0, maximum, std::clamp(current_value, 0, maximum), page,
        font_height / 2};
}

SkillsPanelScrollbarGeometry layout_skills_panel_scrollbar(
    const SkillsPanelScrollRect& bounds,
    const SkillsPanelScrollRange& range) {
    SkillsPanelScrollbarGeometry geometry;
    if (bounds.width <= 0 || bounds.height <= 0) return geometry;

    const auto arrow_height = skills_panel_scroll_arrow_height;
    const auto track_height = std::max(0, bounds.height - arrow_height * 2);
    geometry.up_arrow = {bounds.x, bounds.y, bounds.width, arrow_height};
    geometry.track = {bounds.x, bounds.y + arrow_height, bounds.width, track_height};
    geometry.down_arrow = {bounds.x, bounds.y + bounds.height - arrow_height,
        bounds.width, arrow_height};

    const auto range_size = static_cast<std::int64_t>(range.maximum) - range.minimum;
    const auto page = std::max(0, range.page);
    const auto content_size = range_size + page;
    if (range_size <= 0 || page <= 0 || track_height <= 0 || content_size <= 0)
        return geometry;

    const auto proportion = static_cast<float>(page) / static_cast<float>(content_size);
    const auto proportional_height = static_cast<int>(std::nearbyint(
        static_cast<long double>(track_height) * proportion));
    const auto minimum_thumb_height = std::min(
        skills_panel_scroll_thumb_cap_height * 2, track_height);
    const auto thumb_height = std::clamp(proportional_height,
        minimum_thumb_height, track_height);
    const auto travel = track_height - thumb_height;
    const auto value = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(range.value) - range.minimum, 0, range_size);
    const auto thumb_offset = static_cast<int>(value * travel / range_size);
    geometry.thumb_visible = true;
    geometry.thumb = {bounds.x, geometry.track.y + thumb_offset,
        bounds.width, thumb_height};
    return geometry;
}

int skills_panel_scroll_by_lines(
    const SkillsPanelScrollRange& range,
    int direction,
    int line_count) {
    const auto delta = static_cast<std::int64_t>(direction) * range.line_step *
        std::max(0, line_count);
    return static_cast<int>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(range.value) + delta,
        range.minimum, range.maximum));
}

int skills_panel_scroll_by_pages(
    const SkillsPanelScrollRange& range,
    int direction,
    int page_count) {
    const auto page_delta = std::max(0, range.page - 1);
    const auto delta = static_cast<std::int64_t>(direction) * page_delta *
        std::max(0, page_count);
    return static_cast<int>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(range.value) + delta,
        range.minimum, range.maximum));
}

int skills_panel_scroll_after_track_click(
    const SkillsPanelScrollRange& range,
    const SkillsPanelScrollbarGeometry& geometry,
    int pointer_y) {
    if (!geometry.thumb_visible) return range.value;
    const auto thumb_top = geometry.thumb.y;
    const auto thumb_bottom = thumb_top + geometry.thumb.height;
    if (pointer_y < thumb_top) return skills_panel_scroll_by_pages(range, -1);
    if (pointer_y > thumb_bottom) return skills_panel_scroll_by_pages(range, 1);
    return range.value;
}

int skills_panel_scroll_after_thumb_drag(
    const SkillsPanelScrollRange& range,
    int track_height,
    int thumb_height,
    int grab_offset,
    int pointer_y) {
    const auto range_size = static_cast<std::int64_t>(range.maximum) - range.minimum;
    const auto denominator = static_cast<std::int64_t>(track_height) - thumb_height + 1;
    if (range_size <= 0 || denominator <= 0) return range.minimum;
    const auto numerator = static_cast<std::int64_t>(pointer_y - grab_offset) *
        (range_size + 1);
    const auto value = numerator / denominator + range.minimum;
    return static_cast<int>(std::clamp<std::int64_t>(
        value, range.minimum, range.maximum));
}

}
