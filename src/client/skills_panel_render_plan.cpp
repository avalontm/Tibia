#include "client/skills_panel_render_plan.hpp"

#include <algorithm>

namespace tibia::client {

int skills_panel_natural_content_height(
    const GamePlayerStatistics& statistics,
    const SkillsPanelBarVisibility& bar_visibility,
    int font_height) {
    const auto rows = make_skills_panel_rows(statistics, bar_visibility);
    const auto text_height = std::max(0, font_height);
    auto height = 0;
    for (const auto& row : rows) {
        height += text_height;
        if (row.progress) height += 9;
    }
    return height;
}

SkillsPanelRenderPlan make_skills_panel_render_plan(
    const GamePlayerStatistics& statistics,
    const SkillsPanelBarVisibility& bar_visibility,
    const SkillsPanelRect& panel,
    const SkillsPanelScrollRect& scrollbar_bounds,
    int font_height,
    int scroll_value) {
    SkillsPanelRenderPlan plan;
    plan.background_skin = 7;
    plan.panel = panel;
    plan.rows = make_skills_panel_rows(statistics, bar_visibility);

    const auto text_height = std::max(0, font_height);
    const auto content_height = skills_panel_natural_content_height(
        statistics, bar_visibility, text_height);
    plan.natural_content_height = content_height;

    const auto viewport_height = std::max(0,
        panel.height - skills_panel_padding_y * 2);
    plan.scroll_range = make_skills_panel_scroll_range(content_height,
        viewport_height, scroll_value, text_height);
    const SkillsPanelMetrics metrics{text_height, plan.scroll_range};
    plan.row_layouts = layout_skills_panel_rows(plan.rows, panel, metrics);
    plan.scrollbar = layout_skills_panel_scrollbar(scrollbar_bounds,
        plan.scroll_range);
    return plan;
}

}
