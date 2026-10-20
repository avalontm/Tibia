#pragma once

#include "client/console_panel_layout.hpp"

namespace tibia::client {

struct NewChannelDialogLayout {
    ConsolePanelRect dialog;
    ConsolePanelRect title;
    ConsolePanelRect channel_label;
    ConsolePanelRect channel_list;
    ConsolePanelRect private_label;
    ConsolePanelRect private_field;
    ConsolePanelRect separator;
    ConsolePanelRect ok_button;
    ConsolePanelRect cancel_button;
};

[[nodiscard]] NewChannelDialogLayout resolve_new_channel_dialog_layout(
    int client_width, int client_height, int unit_scale,
    int offset_x = 0, int offset_y = 0) noexcept;

}
