#pragma once

namespace tibia::client {

struct MessageDialogRect {
    int left{};
    int top{};
    int width{};
    int height{};
};

[[nodiscard]] MessageDialogRect layout_message_dialog(
    int client_width, int client_height, int width, int height,
    int offset_x = 0, int offset_y = 0) noexcept;

}
