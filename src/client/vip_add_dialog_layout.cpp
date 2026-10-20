#include "client/vip_add_dialog_layout.hpp"

#include <algorithm>

namespace tibia::client {

VipAddDialogLayout resolve_vip_add_dialog_layout(
    int client_width,
    int client_height,
    int unit_scale,
    int offset_x,
    int offset_y) noexcept {
    unit_scale = std::max(1, unit_scale);
    const auto s = [unit_scale](int value) { return value * unit_scale; };
    const int width = s(236);
    const int height = s(123);
    const int left = std::clamp((client_width - width) / 2 + offset_x,
        0, std::max(0, client_width - width));
    const int top = std::clamp((client_height - height) / 2 + offset_y,
        0, std::max(0, client_height - height));
    const ContentWindowRect dialog{left, top, width, height};
    const auto child = [dialog, &s](int x, int y, int w, int h) {
        return ContentWindowRect{dialog.x + s(x), dialog.y + s(y), s(w), s(h)};
    };
    return {
        dialog,
        child(18, 32, 200, 14),
        child(18, 47, 200, 16),
        child(127, 93, 43, 20),
        child(180, 93, 43, 20)
    };
}

}
