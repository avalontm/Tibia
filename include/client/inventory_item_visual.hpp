#pragma once

#include "resources/tibia_dat.hpp"

#include <cstdint>
#include <optional>

namespace tibia::client {

struct InventoryItemVariant {
    std::uint16_t pattern_x{1};
    std::uint16_t pattern_y{1};
    std::uint16_t pattern_z{1};
    std::uint16_t phase{1};
};

struct InventoryQuantityPosition {
    int x{};
    int y{};
};

[[nodiscard]] InventoryItemVariant inventory_item_variant(
    const resources::TibiaDatObject& object, std::uint8_t extra,
    std::uint32_t animation_clock) noexcept;
[[nodiscard]] std::optional<std::uint8_t> inventory_item_quantity_label(
    const resources::TibiaDatObject& object, std::uint8_t extra) noexcept;
[[nodiscard]] std::optional<std::uint8_t> inventory_item_secondary_quantity_label(
    const resources::TibiaDatObject& object, std::uint8_t extra) noexcept;
[[nodiscard]] InventoryQuantityPosition inventory_quantity_position(
    int item_x, int item_y, int text_width) noexcept;
[[nodiscard]] InventoryQuantityPosition inventory_secondary_quantity_position(
    int item_x, int item_y, int text_width) noexcept;

}
