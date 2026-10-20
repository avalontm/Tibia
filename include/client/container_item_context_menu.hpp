#pragma once

#include "client/content_window_stack_layout.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace tibia::client {

enum class ContainerItemContextAction : std::uint16_t {
    look = 0x2711,
    use = 0x2712,
    use_with = 0x2713,
    open = 0x271b,
    open_in_new_window = 0x271c,
    trade_with = 0x271d,
    move_up = 0x271a,
    rotate = 0x271e
};

struct ContainerItemContextOption {
    ContainerItemContextAction action{};
    std::wstring label;
    ContentWindowRect rect;
};

struct ContainerItemContextMenuPlan {
    ContentWindowRect window;
    std::vector<ContainerItemContextOption> options;
};

inline constexpr int container_item_context_menu_border = 4;
inline constexpr int container_item_context_menu_text_padding = 2;
inline constexpr int container_item_context_menu_action_width_padding = 44;

[[nodiscard]] std::vector<std::pair<ContainerItemContextAction, std::wstring>>
container_item_context_menu_options(bool has_container_property,
    bool has_use_with_property, bool has_rotate_property,
    bool container_position, bool parent_container_open,
    bool has_trade_with_property = false);

[[nodiscard]] ContainerItemContextMenuPlan make_container_item_context_menu_plan(
    std::vector<std::pair<ContainerItemContextAction, std::wstring>> options,
    std::span<const int> text_widths, int font_height, int anchor_x,
    int anchor_y, int client_width, int client_height);

}
