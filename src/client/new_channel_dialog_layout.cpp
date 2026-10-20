#include "client/new_channel_dialog_layout.hpp"

#include <algorithm>

namespace tibia::client {
namespace {

ConsolePanelRect scaled_rect(int x, int y, int width, int height, int scale,
    int origin_x, int origin_y) noexcept {
    return {origin_x + x * scale, origin_y + y * scale,
        width * scale, height * scale};
}

}

NewChannelDialogLayout resolve_new_channel_dialog_layout(int client_width,
    int client_height, int unit_scale, int offset_x, int offset_y) noexcept {
    unit_scale = std::max(1, unit_scale);
    constexpr int width = 236;
    constexpr int height = 255;
    const int scaled_width = width * unit_scale;
    const int scaled_height = height * unit_scale;
    const int origin_x = std::clamp((client_width - scaled_width) / 2 + offset_x,
        0, std::max(0, client_width - scaled_width));
    const int origin_y = std::clamp((client_height - scaled_height) / 2 + offset_y,
        0, std::max(0, client_height - scaled_height));
    NewChannelDialogLayout layout;
    layout.dialog = {origin_x, origin_y, scaled_width, scaled_height};
    layout.title = scaled_rect(5, 5, 226, 12, unit_scale, origin_x, origin_y);
    layout.channel_label = scaled_rect(18, 32, 200, 14, unit_scale, origin_x, origin_y);
    layout.channel_list = scaled_rect(18, 47, 200, 104, unit_scale, origin_x, origin_y);
    layout.private_label = scaled_rect(18, 164, 200, 14, unit_scale, origin_x, origin_y);
    layout.private_field = scaled_rect(18, 179, 200, 16, unit_scale, origin_x, origin_y);
    layout.separator = scaled_rect(12, 217, 212, 2, unit_scale, origin_x, origin_y);
    layout.ok_button = scaled_rect(128, 226, 43, 20, unit_scale, origin_x, origin_y);
    layout.cancel_button = scaled_rect(181, 226, 43, 20, unit_scale, origin_x, origin_y);
    return layout;
}

}
