#pragma once

#include "client/content_window_stack_layout.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace tibia::client {

enum class ConsoleContextMenuAction : std::uint16_t {
    message_to = 0x2774,
    toggle_ignore = 0x2775,
    invite_to_private_chat = 0x2720,
    exclude_from_private_chat = 0x2721,
    add_to_vip = 0x271f,
    copy_selection = 0x27db,
    copy_name = 0x27dc,
    copy_message = 0x27dd,
    select_all = 0x27de,
    rule_violation = 0x2727
};

struct ConsoleContextMenuItem {
    ConsoleContextMenuAction action{};
    std::wstring label;
    ContentWindowRect rect;
};

struct ConsoleContextMenuPlan {
    ContentWindowRect window;
    std::vector<ConsoleContextMenuItem> items;
};

inline constexpr int console_context_menu_border = 4;
inline constexpr int console_context_menu_row_padding = 2;
inline constexpr int console_context_menu_action_width_padding = 44;

[[nodiscard]] ConsoleContextMenuPlan make_console_context_menu_plan(
    std::vector<std::pair<ConsoleContextMenuAction, std::wstring>> items,
    const std::vector<int>& text_widths, int font_height, int anchor_x,
    int anchor_y, int client_width, int client_height);

}
