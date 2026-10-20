#pragma once

#include "client/content_window_stack_layout.hpp"

namespace tibia::client {

struct MoveItemDialogLayout {
    ContentWindowRect dialog;
    ContentWindowRect title;
    ContentWindowRect prompt;
    ContentWindowRect amount_field;
    ContentWindowRect separator;
    ContentWindowRect ok_button;
    ContentWindowRect cancel_button;
};

[[nodiscard]] MoveItemDialogLayout resolve_move_item_dialog_layout(
    int client_width,
    int client_height,
    int unit_scale,
    int offset_x = 0,
    int offset_y = 0) noexcept;

}
