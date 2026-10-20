#pragma once

#include "client/skills_panel_layout.hpp"

#include <array>
#include <cstdint>

namespace tibia::client {

struct SkillsPanelRenderPlan {
    std::uint16_t background_skin{};
    SkillsPanelRect panel;
    int natural_content_height{};
    std::array<SkillsPanelRow, 16> rows;
    std::array<SkillsPanelRowLayout, 16> row_layouts;
    SkillsPanelScrollRange scroll_range;
    SkillsPanelScrollbarGeometry scrollbar;
};

[[nodiscard]] int skills_panel_natural_content_height(
    const GamePlayerStatistics& statistics,
    const SkillsPanelBarVisibility& bar_visibility,
    int font_height);

[[nodiscard]] SkillsPanelRenderPlan make_skills_panel_render_plan(
    const GamePlayerStatistics& statistics,
    const SkillsPanelBarVisibility& bar_visibility,
    const SkillsPanelRect& panel,
    const SkillsPanelScrollRect& scrollbar_bounds,
    int font_height,
    int scroll_value);

}
