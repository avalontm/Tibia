#pragma once

#include "client/content_window_stack_layout.hpp"

#include <array>

namespace tibia::client {

struct VipEditDialogLayout {
    ContentWindowRect dialog;
    ContentWindowRect title;
    ContentWindowRect player_name;
    ContentWindowRect player_name_separator;
    ContentWindowRect type_label;
    std::array<ContentWindowRect, 11> type_buttons;
    ContentWindowRect description_label;
    ContentWindowRect description_field;
    ContentWindowRect notify_checkbox;
    ContentWindowRect ok_button;
    ContentWindowRect cancel_button;
};

[[nodiscard]] VipEditDialogLayout resolve_vip_edit_dialog_layout(
    int client_width,
    int client_height,
    int unit_scale,
    int offset_x = 0,
    int offset_y = 0) noexcept;

}
