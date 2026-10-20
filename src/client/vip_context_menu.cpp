#include "client/vip_context_menu.hpp"

#include "client/console_context_menu.hpp"

#include <algorithm>

namespace tibia::client {

VipContextMenuPlan make_vip_context_menu_plan(
    std::vector<VipContextMenuOption> options,
    const std::vector<int>& text_widths,
    int font_height,
    int anchor_x,
    int anchor_y,
    int client_width,
    int client_height) {
    const int row_height = std::max(0, font_height) +
        console_context_menu_row_padding * 2;
    int width = vip_context_menu_separator_minimum_width;
    for (const auto text_width : text_widths)
        width = std::max(width, std::max(0, text_width) +
            console_context_menu_action_width_padding);
    width += console_context_menu_border * 2;
    const int separators = static_cast<int>(std::count_if(options.begin(),
        options.end(), [](const auto& option) { return option.separator; }));
    const int height = console_context_menu_border * 2 +
        row_height * static_cast<int>(options.size() - separators) +
        vip_context_menu_separator_height * separators;
    int x = anchor_x;
    int y = anchor_y;
    if (x + width > client_width) x -= width;
    if (y + height > client_height) y -= height;
    x = std::max(0, x);
    y = std::max(0, y);

    VipContextMenuPlan plan;
    plan.window = {x, y, width, height};
    plan.entries.reserve(options.size());
    int row_y = y + console_context_menu_border;
    for (auto& option : options) {
        const int entry_height = option.separator
            ? vip_context_menu_separator_height : row_height;
        plan.entries.push_back({option.action, std::move(option.label),
            {x + console_context_menu_border, row_y,
                width - console_context_menu_border * 2, entry_height},
            option.separator});
        row_y += entry_height;
    }
    return plan;
}

}

