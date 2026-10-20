#include "client/container_item_context_menu.hpp"

#include <algorithm>
#include <utility>

namespace tibia::client {

std::vector<std::pair<ContainerItemContextAction, std::wstring>>
container_item_context_menu_options(bool has_container_property,
    bool has_use_with_property, bool has_rotate_property,
    bool container_position, bool parent_container_open,
    bool has_trade_with_property) {
    using Action = ContainerItemContextAction;
    std::vector<std::pair<Action, std::wstring>> options;
    options.emplace_back(Action::look, L"Look");
    if (has_container_property) {
        options.emplace_back(Action::open, L"Open");
        if (container_position)
            options.emplace_back(Action::open_in_new_window,
                L"Open in new window");
    } else if (has_use_with_property) {
        options.emplace_back(Action::use_with, L"Use with ...");
    } else {
        options.emplace_back(Action::use, L"Use");
    }
    if (has_rotate_property)
        options.emplace_back(Action::rotate, L"Rotate");
    if (has_trade_with_property)
        options.emplace_back(Action::trade_with, L"Trade with ...");
    if (container_position && parent_container_open)
        options.emplace_back(Action::move_up, L"Move up");
    return options;
}

ContainerItemContextMenuPlan make_container_item_context_menu_plan(
    std::vector<std::pair<ContainerItemContextAction, std::wstring>> options,
    std::span<const int> text_widths, int font_height, int anchor_x,
    int anchor_y, int client_width, int client_height) {
    const auto row_height = std::max(0, font_height) +
        container_item_context_menu_text_padding * 2;
    int width = 0;
    for (const auto text_width : text_widths)
        width = std::max(width, std::max(0, text_width) +
            container_item_context_menu_action_width_padding);
    width += container_item_context_menu_border * 2;
    const auto height = container_item_context_menu_border * 2 + row_height *
        static_cast<int>(options.size());
    auto x = anchor_x;
    auto y = anchor_y;
    if (x + width > client_width) x -= width;
    if (y + height > client_height) y -= height;
    x = std::clamp(x, 0, std::max(0, client_width - width));
    y = std::clamp(y, 0, std::max(0, client_height - height));

    ContainerItemContextMenuPlan plan;
    plan.window = {x, y, width, height};
    plan.options.reserve(options.size());
    auto row_y = y + container_item_context_menu_border;
    for (auto& option : options) {
        plan.options.push_back({option.first, std::move(option.second),
            {x + container_item_context_menu_border, row_y,
                width - container_item_context_menu_border * 2, row_height}});
        row_y += row_height;
    }
    return plan;
}

}
