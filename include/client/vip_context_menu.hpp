#pragma once

#include "client/content_window_stack_layout.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace tibia::client {

enum class VipContextAction : std::uint16_t {
    add_new = 0x283c,
    remove = 0x283d,
    edit = 0x283e,
    message = 0x283f,
    sort_name = 0x2840,
    sort_type = 0x2841,
    sort_status = 0x2842,
    toggle_offline = 0x2843,
    copy_name = 0x27dc,
    invite_private = 0x2720,
    rule_violation = 0x2727
};

struct VipContextMenuOption {
    VipContextAction action{};
    std::wstring label;
    bool separator{};
};

struct VipContextMenuEntry {
    VipContextAction action{};
    std::wstring label;
    ContentWindowRect rect;
    bool separator{};
};

struct VipContextMenuPlan {
    ContentWindowRect window;
    std::vector<VipContextMenuEntry> entries;
};

inline constexpr int vip_context_menu_separator_height = 8;
inline constexpr int vip_context_menu_separator_minimum_width = 50;

[[nodiscard]] VipContextMenuPlan make_vip_context_menu_plan(
    std::vector<VipContextMenuOption> options,
    const std::vector<int>& text_widths,
    int font_height,
    int anchor_x,
    int anchor_y,
    int client_width,
    int client_height);

}

