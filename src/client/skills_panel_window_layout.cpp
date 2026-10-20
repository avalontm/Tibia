#include "client/skills_panel_window_layout.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <vector>

namespace tibia::client {

std::optional<SkillsPanelWindowPlan> make_skills_panel_window_plan(
    int client_width,
    int client_height,
    int origin_y,
    std::span<const RestoredContentWindow> restored_windows,
    const GamePlayerStatistics& statistics,
    const SkillsPanelBarVisibility& bar_visibility,
    int font_height,
    int scroll_value) {
    const auto skills = std::find_if(restored_windows.begin(), restored_windows.end(),
        [](const auto& window) {
            return window.type == ContentWindowType::skills;
        });
    if (skills == restored_windows.end()) return std::nullopt;

    const auto skills_index = static_cast<std::size_t>(
        std::distance(restored_windows.begin(), skills));
    std::vector<int> heights;
    heights.reserve(skills_index + 1);
    for (std::size_t index = 0; index < skills_index; ++index) {
        const auto height = restored_windows[index].saved_height;
        if (height <= 0) return std::nullopt;
        heights.push_back(height);
    }

    const auto natural_body_height = skills_panel_natural_content_height(
        statistics, bar_visibility, font_height);
    const auto natural_window_height = natural_body_height +
        content_window_frame_header_height;
    const auto minimum_height = content_window_frame_header_height;
    const auto available_height = std::max(minimum_height,
        client_height - origin_y - std::accumulate(heights.begin(), heights.end(), 0));
    const auto window_height = std::min(available_height, resolve_content_window_height(
        skills->saved_height, natural_window_height,
        minimum_height));
    heights.push_back(window_height);
    const auto stack = layout_main_content_window_stack(client_width, heights, origin_y);

    SkillsPanelWindowPlan plan;
    plan.window = stack.back();
    plan.frame = layout_content_window_frame(plan.window);
    plan.resize_grip = {
        plan.window.x + skills_panel_resize_grip_left_offset,
        plan.window.y + plan.window.height - skills_panel_resize_grip_bottom_offset,
        skills_panel_resize_grip_size,
        skills_panel_resize_grip_size};
    plan.resize_hit_area = {
        plan.window.x,
        plan.window.y + plan.window.height - skills_panel_resize_hit_area_height,
        plan.window.width,
        skills_panel_resize_hit_area_height};
    const auto& body = plan.frame.body;
    const SkillsPanelRect panel{body.x, body.y, body.width, body.height};
    const SkillsPanelScrollRect scrollbar{
        body.x + body.width - skills_panel_scrollbar_width,
        body.y, skills_panel_scrollbar_width, body.height};
    plan.skills = make_skills_panel_render_plan(statistics, bar_visibility,
        panel, scrollbar, std::max(0, font_height), scroll_value);
    return plan;
}

}
