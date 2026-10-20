#include "client/container_item_interaction.hpp"

#include <algorithm>
#include <limits>

namespace tibia::client {

namespace {

bool contains(const ContentWindowRect& rect, int x, int y) noexcept {
    return x >= rect.x && y >= rect.y &&
        x < rect.x + rect.width && y < rect.y + rect.height;
}

}

ContainerItemAction resolve_container_item_action(
    std::uint32_t event_flags,
    bool classic_control,
    bool has_dat_property_7) noexcept {
    if ((event_flags & 0x200U) != 0)
        return ContainerItemAction::send_look_request;

    const bool use_directly = classic_control
        ? (event_flags & 0x400U) == 0
        : (event_flags & 0x400U) != 0;
    if (!use_directly || has_dat_property_7)
        return ContainerItemAction::open_context_menu;

    return ContainerItemAction::send_use_request;
}

std::uint32_t container_item_event_flags(
    bool shift_down,
    bool control_down,
    bool alt_down) noexcept {
    return (shift_down ? 0x200U : 0U) |
        (control_down ? 0x400U : 0U) |
        (alt_down ? 0x800U : 0U);
}

const ContainerPanelLayout* container_panel_at(
    std::span<const ContainerPanelLayout> panels,
    int x,
    int y) noexcept {
    for (auto panel = panels.rbegin(); panel != panels.rend(); ++panel)
        if (contains(panel->window, x, y)) return &*panel;
    return nullptr;
}

std::optional<ContainerItemHit> container_item_at(
    std::span<const ContainerPanelLayout> panels,
    std::span<const network::State6Container> containers,
    int x,
    int y) noexcept {
    const auto* panel = container_panel_at(panels, x, y);
    if (panel == nullptr || panel->minimized ||
        !contains(panel->item_viewport, x, y))
        return std::nullopt;
    const auto container = std::find_if(containers.begin(), containers.end(),
        [panel](const auto& item) { return item.id == panel->id; });
    if (container == containers.end()) return std::nullopt;
    for (const auto& cell : panel->item_cells) {
        if (!contains(cell.bounds, x, y) ||
            cell.item_index >= container->items.size() ||
            cell.item_index > std::numeric_limits<std::uint8_t>::max())
            continue;
        const auto& item = container->items[cell.item_index];
        const auto item_type = item.type;
        if (item_type == 0) return std::nullopt;
        return ContainerItemHit{panel->id,
            static_cast<std::uint8_t>(cell.item_index), item_type,
            item.extra, item.has_extra};
    }
    return std::nullopt;
}

std::optional<ContainerItemSlotHit> container_item_slot_at(
    std::span<const ContainerPanelLayout> panels,
    int x,
    int y) noexcept {
    const auto* panel = container_panel_at(panels, x, y);
    if (panel == nullptr || panel->minimized ||
        !contains(panel->item_viewport, x, y))
        return std::nullopt;
    const auto offset_x = x - panel->item_viewport.x;
    const auto offset_y = y - panel->item_viewport.y + panel->scroll_value;
    const auto column = offset_x / container_item_step;
    if (column < 0 || column >= container_column_count ||
        offset_x % container_item_step >= container_item_size ||
        offset_y < 0 || offset_y % container_item_step >= container_item_size)
        return std::nullopt;
    const auto row = offset_y / container_item_step;
    const auto slot = row * container_column_count + column;
    if (slot > std::numeric_limits<std::uint8_t>::max())
        return std::nullopt;
    return ContainerItemSlotHit{panel->id, static_cast<std::uint8_t>(slot)};
}

}
