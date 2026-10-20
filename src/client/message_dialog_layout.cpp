#include "client/message_dialog_layout.hpp"

#include <algorithm>

namespace tibia::client {

MessageDialogRect layout_message_dialog(int client_width, int client_height,
    int width, int height, int offset_x, int offset_y) noexcept {
    width = std::max(0, width);
    height = std::max(0, height);
    const int max_left = std::max(0, client_width - width);
    const int max_top = std::max(0, client_height - height);
    const int left = std::clamp((client_width - width) / 2 + offset_x, 0, max_left);
    const int top = std::clamp((client_height - height) / 2 + offset_y, 0, max_top);
    return {left, top, width, height};
}

}
