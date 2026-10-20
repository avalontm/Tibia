#include "client/battle_panel_window_layout.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <vector>

namespace tibia::client {

std::optional<BattlePanelWindowPlan> make_battle_panel_window_plan(
    int client_width,
    int client_height,
    int origin_y,
    std::span<const RestoredContentWindow> restored_windows,
    std::span<const BattleCreatureRow> rows,
    int scroll_offset) {
    const auto battle = std::find_if(restored_windows.begin(), restored_windows.end(),
        [](const auto& window) {
            return window.type == ContentWindowType::battle;
        });
    if (battle == restored_windows.end()) return std::nullopt;

    const auto battle_index = static_cast<std::size_t>(
        std::distance(restored_windows.begin(), battle));
    std::vector<int> heights;
    heights.reserve(battle_index + 1);
    for (std::size_t index = 0; index < battle_index; ++index) {
        const auto height = restored_windows[index].saved_height;
        if (height <= 0) return std::nullopt;
        heights.push_back(height);
    }

    const auto body_height = std::max(
        static_cast<int>(rows.size()) * battle_panel_row_height + 1,
        battle_panel_row_height);
    const auto natural_window_height = body_height +
        content_window_frame_header_height;
    const auto desired_height = battle->saved_height ==
            content_window_frame_header_height
        ? content_window_frame_header_height
        : resolve_content_window_height(battle->saved_height,
            natural_window_height,
            content_window_frame_header_height + battle_panel_row_height);
    const auto available_height = std::max(
        content_window_frame_header_height + battle_panel_row_height,
        client_height - origin_y -
            std::accumulate(heights.begin(), heights.end(), 0));
    const auto window_height = std::min(available_height, desired_height);
    heights.push_back(window_height);
    const auto stack = layout_main_content_window_stack(client_width, heights, origin_y);

    BattlePanelWindowPlan plan;
    plan.window = stack.back();
    plan.frame = layout_content_window_frame(plan.window);
    const auto& body = plan.frame.body;
    const BattlePanelRect viewport{body.x, body.y,
        std::max(0, body.width - battle_panel_scrollbar_width), body.height};
    plan.panel = layout_battle_panel_rows(rows, viewport, scroll_offset);
    plan.scrollbar = layout_battle_panel_scrollbar(
        {body.x + body.width - battle_panel_scrollbar_width, body.y,
            battle_panel_scrollbar_width, body.height},
        plan.panel.content_height, viewport.height, plan.panel.scroll_offset);
    return plan;
}

}
