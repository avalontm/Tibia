#include "client/skills_panel_layout.hpp"

#include <algorithm>
#include <cstddef>

namespace tibia::client {
namespace {

constexpr int progress_track_height = 5;
constexpr int progress_track_top_after_font = 8;
constexpr int progress_row_spacing = 10;

SkillsPanelRgb progress_color(std::size_t row, std::uint32_t progress) {
    if (row == 6)
        return progress < 25 ? SkillsPanelRgb{192, 192, 0}
                             : SkillsPanelRgb{192, 96, 0};
    if (row > 8) return {0, 192, 0};
    return {192, 0, 0};
}

SkillsPanelRect intersect(const SkillsPanelRect& left, const SkillsPanelRect& right) {
    const auto x = std::max(left.x, right.x);
    const auto y = std::max(left.y, right.y);
    const auto right_edge = std::min(left.x + left.width, right.x + right.width);
    const auto bottom_edge = std::min(left.y + left.height, right.y + right.height);
    return {x, y, std::max(0, right_edge - x), std::max(0, bottom_edge - y)};
}

}

std::array<SkillsPanelRowLayout, 16> layout_skills_panel_rows(
    const std::array<SkillsPanelRow, 16>& rows,
    const SkillsPanelRect& panel,
    const SkillsPanelMetrics& metrics) {
    std::array<SkillsPanelRowLayout, 16> result{};
    const SkillsPanelRect panel_clip{panel.x, panel.y, panel.width, panel.height};
    const auto available_width = std::max(0,
        panel.width - skills_panel_padding_x * 2 - skills_panel_scrollbar_width);
    auto row_offset = -metrics.scroll.value;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        auto& layout = result[index];
        const auto& row = rows[index];
        layout.kind = row.kind;
        if (row.progress) layout.progress_color = progress_color(index, *row.progress);
        const auto row_height = metrics.font_height +
            (row.progress ? progress_row_spacing : 0);
        const auto row_top = panel.y + row_offset;
        layout.clip = intersect({panel.x + skills_panel_padding_x,
            row_top + skills_panel_padding_y, available_width, row_height}, panel_clip);
        layout.label_x = panel.x + skills_panel_padding_x;
        layout.label_y = row_top + skills_panel_padding_y;
        layout.value_right = panel.x + skills_panel_padding_x + available_width - 1;
        layout.value_y = row_top + skills_panel_padding_y;
        layout.progress_track = {panel.x + skills_panel_padding_x,
            row_top + metrics.font_height + progress_track_top_after_font,
            available_width, progress_track_height};
        layout.separator_y = row_top + metrics.font_height / 2 +
            skills_panel_padding_y - 1;
        if (row.progress && available_width > 2) {
            const auto fill_width = static_cast<int>(
                static_cast<std::int64_t>(available_width - 2) *
                std::min(*row.progress, 100U) / 100U);
            layout.progress_fill = {layout.progress_track.x + 1,
                layout.progress_track.y + 1, fill_width, progress_track_height - 2};
        }
        row_offset += row_height;
    }
    return result;
}

}
