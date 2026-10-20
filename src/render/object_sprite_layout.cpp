#include "render/object_sprite_layout.hpp"

namespace tibia::render {
namespace {

inline constexpr std::int32_t sprite_tile_pixels = 32;

std::uint16_t positive_pattern(std::int32_t value, std::uint8_t dimension) {
    if (dimension == 0) return 0;
    const auto remainder = value % dimension;
    return static_cast<std::uint16_t>((remainder < 0 ? remainder + dimension : remainder) + 1);
}

std::uint32_t fluid_pattern(std::uint32_t subtype) noexcept {
    switch (subtype) {
    case 0: return 0;
    case 2:
    case 10: return 7;
    case 3:
    case 4:
    case 7:
    case 13: return 3;
    case 5:
    case 11: return 2;
    case 6: return 4;
    case 8:
    case 12:
    case 14: return 5;
    case 9:
    case 15: return 6;
    default: return 1;
    }
}

}

std::optional<ObjectSpriteVariant> resolve_object_sprite_variant(
    const resources::TibiaDatObject& object,
    const ObjectSpriteVariantContext& context) {
    if (object.pattern_x == 0 || object.pattern_y == 0 ||
        object.pattern_z == 0 || object.animation_frames == 0)
        return std::nullopt;

    ObjectSpriteVariant variant{
        positive_pattern(context.world_x, object.pattern_x),
        positive_pattern(context.world_y, object.pattern_y),
        positive_pattern(context.world_floor, object.pattern_z),
        static_cast<std::uint16_t>((context.animation_clock / 5U) %
            object.animation_frames + 1U)
    };

    if (object.has_native_property(5)) {
        if (!context.cumulative_value) return std::nullopt;
        const auto value = *context.cumulative_value;
        std::uint32_t pattern_x = 0;
        std::uint32_t pattern_y = 0;
        if (value == 2) pattern_x = 1;
        else if (value == 3) pattern_x = 2;
        else if (value == 4) pattern_x = 3;
        else if (value > 4) {
            pattern_x = value < 10 ? 0 : value < 25 ? 1 : value < 50 ? 2 : 3;
            pattern_y = 1;
        }
        variant.pattern_x = static_cast<std::uint16_t>(pattern_x % object.pattern_x + 1);
        variant.pattern_y = static_cast<std::uint16_t>(pattern_y % object.pattern_y + 1);
    }

    if (object.has_native_property(11) || object.has_native_property(12)) {
        if (!context.subtype) return std::nullopt;
        const auto mapped = fluid_pattern(*context.subtype);
        variant.pattern_x = static_cast<std::uint16_t>((mapped & 3U) % object.pattern_x + 1);
        variant.pattern_y = static_cast<std::uint16_t>((mapped >> 2U) % object.pattern_y + 1);
    }

    if (object.has_native_property(18)) {
        if (!context.hanging_selector) return std::nullopt;
        std::uint32_t pattern_x = 0;
        if (*context.hanging_selector == 0x13U && object.pattern_x > 1)
            pattern_x = 1;
        else if (*context.hanging_selector == 0x14U && object.pattern_x > 2)
            pattern_x = 2;
        variant.pattern_x = static_cast<std::uint16_t>(pattern_x + 1);
        variant.pattern_y = 1;
    }

    return variant;
}

std::optional<ObjectSpriteExtent> resolve_object_sprite_extent(
    const resources::TibiaDatObject& object) noexcept {
    if (!object.has_native_property(0x19)) return ObjectSpriteExtent{};
    const auto* attribute = object.find_attribute(0x19);
    if (attribute == nullptr || attribute->payload_words.size() != 2)
        return std::nullopt;
    return ObjectSpriteExtent{
        attribute->payload_words[0],
        attribute->payload_words[1]
    };
}

std::vector<ObjectSpritePlacement> resolve_object_sprite_layout(
    const resources::TibiaDatObject& object,
    std::uint16_t pattern_x, std::uint16_t pattern_y,
    std::uint16_t pattern_z, std::uint16_t phase,
    std::int32_t extent_x, std::int32_t extent_y) {
    std::vector<ObjectSpritePlacement> placements;
    for (std::uint16_t cell_x = 1; cell_x <= object.width; ++cell_x) {
        for (std::uint16_t cell_y = 1; cell_y <= object.height; ++cell_y) {
            for (std::uint16_t layer = 1; layer <= object.layers; ++layer) {
                const auto sprite_id = object.sprite_id_at(cell_x, cell_y,
                    layer, pattern_x, pattern_y, pattern_z, phase);
                if (sprite_id == 0) continue;
                placements.push_back({
                    sprite_id,
                    cell_x,
                    cell_y,
                    layer,
                    sprite_tile_pixels - static_cast<std::int32_t>(cell_x) * sprite_tile_pixels - extent_x,
                    sprite_tile_pixels - static_cast<std::int32_t>(cell_y) * sprite_tile_pixels - extent_y,
                    object.animation_frames > 1
                });
            }
        }
    }
    return placements;
}

std::optional<std::vector<ObjectSpritePlacement>> resolve_object_sprite_layout(
    const resources::TibiaDatObject& object, const ObjectSpriteVariant& variant) {
    const auto extent = resolve_object_sprite_extent(object);
    if (!extent) return std::nullopt;
    return resolve_object_sprite_layout(object, variant.pattern_x, variant.pattern_y,
        variant.pattern_z, variant.phase, extent->x, extent->y);
}

}
