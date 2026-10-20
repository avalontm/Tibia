#include "client/inventory_item_visual.hpp"

#include <algorithm>

namespace tibia::client {
namespace {

std::uint16_t one_based_pattern(std::uint32_t value, std::uint8_t dimension) noexcept {
    return static_cast<std::uint16_t>(value % std::max<std::uint8_t>(dimension, 1) + 1);
}

}

InventoryItemVariant inventory_item_variant(
    const resources::TibiaDatObject& object, std::uint8_t extra,
    std::uint32_t animation_clock) noexcept {
    InventoryItemVariant variant;
    if (object.animation_frames > 0)
        variant.phase = static_cast<std::uint16_t>(
            (animation_clock / 5U) % object.animation_frames + 1U);
    if (object.has_native_property(5)) {
        std::uint32_t stack_pattern{};
        std::uint32_t depth_pattern{};
        if (extra == 2) stack_pattern = 1;
        else if (extra == 3) stack_pattern = 2;
        else if (extra == 4) stack_pattern = 3;
        else if (extra >= 5) {
            stack_pattern = extra < 10 ? 0 : extra < 25 ? 1 : extra < 50 ? 2 : 3;
            depth_pattern = 1;
        }
        variant.pattern_x = one_based_pattern(stack_pattern, object.pattern_x);
        variant.pattern_z = one_based_pattern(depth_pattern, object.pattern_z);
    }
    if (object.has_native_property(12) || object.has_native_property(11)) {
        const auto category = extra == 0 ? 0U : extra == 2 || extra == 10 ? 7U :
            extra == 3 || extra == 4 || extra == 7 || extra == 13 ? 3U :
            extra == 5 || extra == 11 ? 2U : extra == 6 ? 4U :
            extra == 8 || extra == 12 || extra == 14 ? 5U :
            extra == 9 || extra == 15 ? 6U : 1U;
        variant.pattern_x = one_based_pattern(category & 3U, object.pattern_x);
        variant.pattern_z = one_based_pattern(category >> 2U, object.pattern_z);
    }
    return variant;
}

std::optional<std::uint8_t> inventory_item_quantity_label(
    const resources::TibiaDatObject& object, std::uint8_t extra) noexcept {
    if (!object.has_native_property(5) || extra <= 1) return std::nullopt;
    return extra;
}

std::optional<std::uint8_t> inventory_item_secondary_quantity_label(
    const resources::TibiaDatObject& object, std::uint8_t extra) noexcept {
    if (!object.has_native_property(8) || extra <= 1) return std::nullopt;
    return extra;
}

InventoryQuantityPosition inventory_quantity_position(
    int item_x, int item_y, int text_width) noexcept {
    return {item_x + 31 - text_width, item_y + 31 - text_width};
}

InventoryQuantityPosition inventory_secondary_quantity_position(
    int item_x, int item_y, int text_width) noexcept {
    return {item_x + 31 - text_width, item_y + 1};
}

}
