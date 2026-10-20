#pragma once

#include "client/content_window_stack_layout.hpp"

namespace tibia::client {

struct VipAddDialogLayout {
    ContentWindowRect dialog;
    ContentWindowRect name_label;
    ContentWindowRect name_field;
    ContentWindowRect ok_button;
    ContentWindowRect cancel_button;
};

[[nodiscard]] VipAddDialogLayout resolve_vip_add_dialog_layout(
    int client_width,
    int client_height,
    int unit_scale,
    int offset_x,
    int offset_y) noexcept;

}
