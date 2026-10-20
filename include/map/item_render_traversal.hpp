#pragma once

#include "map/model.hpp"
#include "map/tile_object_order.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace tibia::resources {
struct TibiaDatFile;
}

namespace tibia::map {

enum class MapItemTraversalStatus : std::uint8_t {
    complete,
    unresolved_tile,
    unresolved_dat_object
};

struct MapItemRenderVisit {
    const Tile* tile{};
    std::int32_t local_x{};
    std::int32_t local_y{};
    std::int32_t anchor_x{};
    std::int32_t anchor_y{};
    std::uint8_t floor{};
    bool neighboring_object_pass{};
    struct SpriteObject {
        const Object* object{};
        std::int32_t anchor_adjustment{};
    };
    std::vector<const Object*> ordered_objects;
    std::vector<SpriteObject> sprite_objects;
    const Object* deferred_object{};
};

struct MapItemTraversal {
    MapItemTraversalStatus status{MapItemTraversalStatus::complete};
    std::vector<MapItemRenderVisit> visits;
};

struct DeferredMapItemDraw {
    const Object* object{};
    std::int32_t anchor_x{};
    std::int32_t anchor_y{};
};

[[nodiscard]] MapItemTraversal collect_map_item_render_visits(
    const DecodedMap& map,
    const resources::TibiaDatFile& dat,
    std::int32_t local_x,
    std::int32_t local_y,
    std::uint8_t floor,
    std::int32_t anchor_x,
    std::int32_t anchor_y);

[[nodiscard]] std::optional<DeferredMapItemDraw> resolve_deferred_map_item_draw(
    const MapItemRenderVisit& visit,
    std::int32_t effect_index) noexcept;

}
