#pragma once

#include "client/content_window_frame_layout.hpp"
#include "client/content_window_state.hpp"
#include "client/skills_panel_render_plan.hpp"

#include <optional>
#include <span>

namespace tibia::client {

struct SkillsPanelWindowPlan {
    ContentWindowRect window;
    ContentWindowFrameLayout frame;
    ContentWindowRect resize_grip;
    ContentWindowRect resize_hit_area;
    SkillsPanelRenderPlan skills;
};

inline constexpr int skills_panel_resize_grip_size = 13;
inline constexpr int skills_panel_resize_grip_left_offset = 3;
inline constexpr int skills_panel_resize_grip_bottom_offset = 16;
inline constexpr int skills_panel_resize_hit_area_height = 4;

[[nodiscard]] std::optional<SkillsPanelWindowPlan> make_skills_panel_window_plan(
    int client_width,
    int client_height,
    int origin_y,
    std::span<const RestoredContentWindow> restored_windows,
    const GamePlayerStatistics& statistics,
    const SkillsPanelBarVisibility& bar_visibility,
    int font_height,
    int scroll_value);

}
