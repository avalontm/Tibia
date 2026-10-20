#include "client/move_item_dialog_layout.hpp"

#include <algorithm>

namespace tibia::client {

MoveItemDialogLayout resolve_move_item_dialog_layout(
    int client_width,
    int client_height,
    int unit_scale,
    int offset_x,
    int offset_y) noexcept {
    unit_scale = std::max(1, unit_scale);
    const auto s = [unit_scale](int value) { return value * unit_scale; };
    constexpr int width = 236;
    constexpr int height = 123;
    const int dialog_x = std::clamp((client_width - s(width)) / 2 + offset_x,
        0, std::max(0, client_width - s(width)));
    const int dialog_y = std::clamp((client_height - s(height)) / 2 + offset_y,
        0, std::max(0, client_height - s(height)));
    const ContentWindowRect dialog{dialog_x, dialog_y, s(width), s(height)};
    const auto child = [dialog, &s](int x, int y, int w, int h) {
        return ContentWindowRect{dialog.x + s(x), dialog.y + s(y), s(w), s(h)};
    };
    return {
        dialog,
        child(5, 5, 226, 18),
        child(63, 34, 155, 28),
        child(63, 64, 155, 12),
        child(12, 83, 212, 2),
        child(127, 93, 43, 20),
        child(180, 93, 43, 20)
    };
}

}
