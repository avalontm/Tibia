#include "client/console_context_menu.hpp"

#include <algorithm>
#include <utility>

namespace tibia::client {

ConsoleContextMenuPlan make_console_context_menu_plan(
    std::vector<std::pair<ConsoleContextMenuAction, std::wstring>> items,
    const std::vector<int>& text_widths, int font_height, int anchor_x,
    int anchor_y, int client_width, int client_height) {
    const int row_height = std::max(0, font_height) +
        console_context_menu_row_padding * 2;
    int width = 0;
    for (const auto text_width : text_widths)
        width = std::max(width, std::max(0, text_width) +
            console_context_menu_action_width_padding);
    width += console_context_menu_border * 2;
    const int height = console_context_menu_border * 2 +
        row_height * static_cast<int>(items.size());
    int x = anchor_x;
    int y = anchor_y;
    if (x + width > client_width) x -= width;
    if (y + height > client_height) y -= height;
    x = std::max(0, x);
    y = std::max(0, y);

    ConsoleContextMenuPlan plan;
    plan.window = {x, y, width, height};
    plan.items.reserve(items.size());
    int row_y = y + console_context_menu_border;
    for (auto& item : items) {
        plan.items.push_back({item.first, std::move(item.second),
            {x + console_context_menu_border, row_y,
                width - console_context_menu_border * 2, row_height}});
        row_y += row_height;
    }
    return plan;
}

}
