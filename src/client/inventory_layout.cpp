#include "client/inventory_layout.hpp"

namespace tibia::client {

std::optional<std::uint8_t> inventory_slot_at(
    int panel_x, int panel_y, int cursor_x, int cursor_y) noexcept {
    const int local_x = cursor_x - panel_x;
    const int local_y = cursor_y - panel_y;
    for (const auto& slot : inventory_slot_layouts) {
        if (local_x >= slot.x && local_y >= slot.y &&
            local_x < slot.x + inventory_item_slot_size &&
            local_y < slot.y + inventory_item_slot_size)
            return slot.slot;
    }
    return std::nullopt;
}

}
