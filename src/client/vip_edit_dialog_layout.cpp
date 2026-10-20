#include "client/vip_edit_dialog_layout.hpp"

#include <algorithm>

namespace tibia::client {

VipEditDialogLayout resolve_vip_edit_dialog_layout(int client_width,
    int client_height, int unit_scale, int offset_x, int offset_y) noexcept {
    unit_scale = std::max(1, unit_scale);
    const auto s = [unit_scale](int value) { return value * unit_scale; };
    const int width = s(286);
    const int height = s(266);
    const int origin_x = std::clamp((client_width - width) / 2 + offset_x,
        0, std::max(0, client_width - width));
    const int origin_y = std::clamp((client_height - height) / 2 + offset_y,
        0, std::max(0, client_height - height));
    const ContentWindowRect dialog{origin_x, origin_y, width, height};
    const auto child = [dialog, &s](int x, int y, int w, int h) {
        return ContentWindowRect{dialog.x + s(18 + x), dialog.y + s(32 + y),
            s(w), s(h)};
    };
    VipEditDialogLayout layout;
    layout.dialog = dialog;
    layout.title = {dialog.x + s(5), dialog.y + s(5), s(276), s(12)};
    layout.player_name = child(0, 0, 200, 14);
    layout.player_name_separator = child(0, 15, 200, 2);
    layout.type_label = child(0, 32, 200, 14);
    for (std::size_t index = 0; index < layout.type_buttons.size(); ++index)
        layout.type_buttons[index] = child(static_cast<int>(index) * 20,
            47, 24, 25);
    layout.description_label = child(0, 76, 200, 14);
    layout.description_field = child(0, 91, 200, 48);
    layout.notify_checkbox = child(0, 152, 200, 22);
    layout.ok_button = {dialog.x + s(177), dialog.y + s(236), s(43), s(20)};
    layout.cancel_button = {dialog.x + s(230), dialog.y + s(236), s(43), s(20)};
    return layout;
}

}
