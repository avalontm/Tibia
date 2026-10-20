#include "render/map_item_layout.hpp"

#include "resources/tibia_dat.hpp"

namespace tibia::render {

std::optional<MapItemSpriteLayout> resolve_map_item_sprite_layout(
    const map::Object& map_object,
    const map::Tile& tile,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock) {
    return resolve_map_item_sprite_layout(
        map_object, tile, dat, animation_clock, {});
}

std::optional<MapItemSpriteLayout> resolve_map_item_sprite_layout(
    const map::Object& map_object,
    const map::Tile& tile,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock,
    const MapItemSpriteVariantInputs& variant_inputs) {
    if (map_object.type == 0 || map_object.creature_id != 0) return std::nullopt;
    const auto* item = dat.find_item(map_object.type);
    if (item == nullptr) return std::nullopt;

    const auto position = variant_inputs.position.value_or(
        MapItemSpriteVariantInputs::Position{tile.x, tile.y, tile.floor});
    const ObjectSpriteVariantContext context{
        position.x,
        position.y,
        position.floor,
        animation_clock,
        variant_inputs.cumulative_value,
        variant_inputs.subtype,
        variant_inputs.hanging_selector
    };
    const auto variant = resolve_object_sprite_variant(*item, context);
    if (!variant) return std::nullopt;
    const auto placements = resolve_object_sprite_layout(*item, *variant);
    if (!placements) return std::nullopt;

    return MapItemSpriteLayout{map_object.type, *variant, *placements};
}

std::optional<MapItemSpriteVariantInputs> resolve_map_item_sprite_variant_inputs(
    const map::Object& map_object,
    const resources::TibiaDatFile& dat) {
    if (map_object.type == 0 || map_object.creature_id != 0)
        return std::nullopt;
    const auto* item = dat.find_item(map_object.type);
    if (item == nullptr) return std::nullopt;
    MapItemSpriteVariantInputs inputs;
    if (item->has_native_property(18))
        inputs.hanging_selector = map_object.render_context;
    const auto needs_extra = item->has_native_property(5) ||
        item->has_native_property(11) || item->has_native_property(12);
    if (needs_extra && !map_object.has_extra) return std::nullopt;
    if (item->has_native_property(5))
        inputs.cumulative_value = map_object.extra;
    if (item->has_native_property(11) || item->has_native_property(12))
        inputs.subtype = map_object.extra;
    return inputs;
}

}
