#pragma once

#include "resources/tibia_dat.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace tibia::render {

struct ObjectSpritePlacement {
    std::uint16_t sprite_id{};
    std::uint16_t cell_x{};
    std::uint16_t cell_y{};
    std::uint16_t layer{};
    std::int32_t offset_x{};
    std::int32_t offset_y{};
    bool bypass_tile_cache{};
};

struct ObjectSpriteVariantContext {
    std::int32_t world_x{};
    std::int32_t world_y{};
    std::int32_t world_floor{};
    std::uint32_t animation_clock{};
    std::optional<std::uint32_t> cumulative_value;
    std::optional<std::uint32_t> subtype;
    std::optional<std::uint32_t> hanging_selector;
};

struct ObjectSpriteVariant {
    std::uint16_t pattern_x{};
    std::uint16_t pattern_y{};
    std::uint16_t pattern_z{};
    std::uint16_t phase{};
};

struct ObjectSpriteExtent {
    std::int32_t x{};
    std::int32_t y{};
};

[[nodiscard]] std::optional<ObjectSpriteVariant> resolve_object_sprite_variant(
    const resources::TibiaDatObject& object,
    const ObjectSpriteVariantContext& context);

[[nodiscard]] std::optional<ObjectSpriteExtent> resolve_object_sprite_extent(
    const resources::TibiaDatObject& object) noexcept;

[[nodiscard]] std::vector<ObjectSpritePlacement> resolve_object_sprite_layout(
    const resources::TibiaDatObject& object,
    std::uint16_t pattern_x, std::uint16_t pattern_y,
    std::uint16_t pattern_z, std::uint16_t phase,
    std::int32_t extent_x, std::int32_t extent_y);

[[nodiscard]] std::optional<std::vector<ObjectSpritePlacement>> resolve_object_sprite_layout(
    const resources::TibiaDatObject& object, const ObjectSpriteVariant& variant);

}
