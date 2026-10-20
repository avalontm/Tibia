#pragma once

#include "client/container_panel_layout.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace tibia::client {

struct ContainerItemHit {
    std::uint8_t container_id{};
    std::uint8_t slot{};
    std::uint16_t item_type{};
    std::uint8_t extra{};
    bool has_extra{};
};

struct ContainerItemSlotHit {
    std::uint8_t container_id{};
    std::uint8_t slot{};
};

enum class ContainerItemAction {
    send_look_request,
    send_use_request,
    open_context_menu
};

[[nodiscard]] ContainerItemAction resolve_container_item_action(
    std::uint32_t event_flags,
    bool classic_control,
    bool has_dat_property_7) noexcept;

[[nodiscard]] std::uint32_t container_item_event_flags(
    bool shift_down,
    bool control_down,
    bool alt_down) noexcept;

[[nodiscard]] const ContainerPanelLayout* container_panel_at(
    std::span<const ContainerPanelLayout> panels,
    int x,
    int y) noexcept;

[[nodiscard]] std::optional<ContainerItemHit> container_item_at(
    std::span<const ContainerPanelLayout> panels,
    std::span<const network::State6Container> containers,
    int x,
    int y) noexcept;

[[nodiscard]] std::optional<ContainerItemSlotHit> container_item_slot_at(
    std::span<const ContainerPanelLayout> panels,
    int x,
    int y) noexcept;

}
