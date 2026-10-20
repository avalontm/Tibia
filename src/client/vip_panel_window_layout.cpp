#include "client/vip_panel_window_layout.hpp"

#include "client/content_window_stack_layout.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

namespace tibia::client {

std::optional<VipPanelWindowPlan> make_vip_panel_window_plan(
    int client_width,
    int client_height,
    int origin_y,
    std::span<const RestoredContentWindow> restored_windows,
    std::span<const network::State6BuddyEntry> contacts,
    int scroll_value) {
    const auto vip = std::find_if(restored_windows.begin(), restored_windows.end(),
        [](const auto& window) { return window.type == ContentWindowType::vip; });
    if (vip == restored_windows.end()) return std::nullopt;

    const auto vip_index = static_cast<std::size_t>(
        std::distance(restored_windows.begin(), vip));
    std::vector<int> heights;
    heights.reserve(vip_index + 1);
    int occupied_height{};
    for (std::size_t index = 0; index < vip_index; ++index) {
        const auto height = restored_windows[index].saved_height;
        if (height <= 0) return std::nullopt;
        heights.push_back(height);
        occupied_height += height;
    }

    const auto row_count = std::min<std::size_t>(contacts.size(),
        static_cast<std::size_t>((std::numeric_limits<int>::max() -
            vip_panel_body_padding) / vip_panel_row_height));
    const int natural_body_height = vip_panel_body_padding +
        static_cast<int>(row_count) * vip_panel_row_height;
    const int minimum_height = content_window_frame_header_height +
        vip_panel_body_padding;
    const int natural_height = content_window_frame_header_height +
        natural_body_height;
    const int remaining_height = std::max(minimum_height,
        client_height - origin_y - occupied_height);
    const int window_height = vip->saved_height == content_window_frame_header_height
        ? content_window_frame_header_height
        : std::min(remaining_height, resolve_content_window_height(
            vip->saved_height, natural_height, minimum_height));
    heights.push_back(window_height);
    const auto stack = layout_main_content_window_stack(client_width, heights, origin_y);

    VipPanelWindowPlan plan;
    plan.window = stack.back();
    plan.frame = layout_content_window_frame(plan.window);
    plan.viewport = {plan.frame.body.x, plan.frame.body.y,
        std::max(0, plan.frame.body.width - vip_panel_scrollbar_width),
        plan.frame.body.height};
    plan.scroll = make_skills_panel_scroll_range(natural_body_height,
        plan.viewport.height, scroll_value, vip_panel_row_height);
    plan.rows.reserve(contacts.size());
    for (std::size_t index = 0; index < contacts.size(); ++index) {
        const int top = plan.viewport.y + static_cast<int>(index) *
            vip_panel_row_height - plan.scroll.value;
        const int clipped_top = std::max(top, plan.viewport.y);
        const int clipped_bottom = std::min(top + vip_panel_row_height,
            plan.viewport.y + plan.viewport.height);
        plan.rows.push_back({
            {plan.viewport.x + vip_panel_text_offset_x,
                top + vip_panel_text_offset_y,
                std::min(vip_panel_text_max_width,
                    std::max(0, plan.viewport.width - vip_panel_text_offset_x)),
                vip_panel_row_height},
            {plan.viewport.x, clipped_top, plan.viewport.width,
                std::max(0, clipped_bottom - clipped_top)},
            {plan.viewport.x + vip_panel_icon_offset,
                top + vip_panel_icon_offset, vip_panel_icon_size,
                vip_panel_icon_size},
            index
        });
    }
    plan.scrollbar = layout_skills_panel_scrollbar(
        {plan.frame.body.x + plan.frame.body.width - vip_panel_scrollbar_width,
            plan.frame.body.y, vip_panel_scrollbar_width, plan.frame.body.height},
        plan.scroll);
    return plan;
}

}
