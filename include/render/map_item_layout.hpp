#pragma once

#include "map/model.hpp"
#include "render/object_sprite_layout.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace tibia::render {

struct MapItemSpriteLayout {
    std::uint16_t item_id{};
    ObjectSpriteVariant variant;
    std::vector<ObjectSpritePlacement> placements;
};

struct MapItemSpriteVariantInputs {
    struct Position {
        std::int32_t x{};
        std::int32_t y{};
        std::int32_t floor{};
    };

    std::optional<std::uint32_t> cumulative_value;
    std::optional<std::uint32_t> subtype;
    std::optional<std::uint32_t> hanging_selector;
    std::optional<Position> position;
};

[[nodiscard]] std::optional<MapItemSpriteVariantInputs>
resolve_map_item_sprite_variant_inputs(
    const map::Object& map_object,
    const resources::TibiaDatFile& dat);

[[nodiscard]] std::optional<MapItemSpriteLayout> resolve_map_item_sprite_layout(
    const map::Object& map_object,
    const map::Tile& tile,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock);

[[nodiscard]] std::optional<MapItemSpriteLayout> resolve_map_item_sprite_layout(
    const map::Object& map_object,
    const map::Tile& tile,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock,
    const MapItemSpriteVariantInputs& variant_inputs);

}
