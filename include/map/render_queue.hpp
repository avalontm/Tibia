#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "map/light_field.hpp"
#include "map/model.hpp"
#include "map/render_cell_filter.hpp"

namespace tibia::map {

inline constexpr std::int32_t map_tile_pixels = 32;
inline constexpr std::int32_t map_viewport_width = 480;
inline constexpr std::int32_t map_viewport_height = 352;
inline constexpr std::int32_t map_viewport_tiles_x = 18;
inline constexpr std::int32_t map_viewport_tiles_y = 14;
inline constexpr std::int32_t map_cache_columns = 16;
inline constexpr std::int32_t map_cache_rows = 12;

struct MapCacheCoordinate {
    std::int32_t column{};
    std::int32_t row{};
    std::size_t index{};
};

enum class TileDrawPath : std::uint8_t {
    direct_sprite,
    textured
};

enum class TileDrawTarget : std::uint8_t {
    tile_cache,
    viewport
};

enum class TileDrawAsset : std::uint8_t {
    tibia_spr,
    tibia_pic
};

struct TileRenderObject {
    std::int32_t draw_selector{};
    std::int32_t offset_x{};
    std::int32_t offset_y{};
    bool operator==(const TileRenderObject&) const = default;
};

struct TileRenderCell {
    std::int32_t origin_x{};
    std::int32_t origin_y{};
    std::vector<TileRenderObject> objects;
    std::size_t current_count{};
    std::size_t cached_count{};
    bool submitted{};
};

struct TileDrawRequest {
    TileDrawPath path{};
    TileDrawTarget target{};
    TileDrawAsset asset{TileDrawAsset::tibia_spr};
    std::int32_t destination_x{};
    std::int32_t destination_y{};
    std::int32_t selector{};
    std::int32_t clip_x{};
    std::int32_t clip_y{};
    std::int32_t clip_width{map_tile_pixels};
    std::int32_t clip_height{map_tile_pixels};
};

struct TileRendererUpdate {
    std::int32_t screen_x{};
    std::int32_t screen_y{};
    std::int32_t cell_x{};
    std::int32_t cell_y{};
};

enum class TileRenderStatus : std::uint8_t {
    outside_view,
    already_submitted,
    unresolved_cell_data,
    queued
};

struct TileRenderBatch {
    TileRenderStatus status{TileRenderStatus::outside_view};
    std::vector<TileDrawRequest> draws;
    TileRendererUpdate update;
    bool reset_cache{};
};

enum class TileObjectUpdateStatus : std::uint8_t {
    outside_view,
    unresolved_cell_data,
    added_to_cache,
    added_to_submitted_cell
};

struct TileObjectUpdateBatch {
    TileObjectUpdateStatus status{TileObjectUpdateStatus::outside_view};
    std::vector<TileDrawRequest> draws;
    bool reset_cache{};
};

struct MapTileRenderBatch {
    std::int32_t local_x{};
    std::int32_t local_y{};
    TileRenderBatch batch;
};

enum class ObjectSpriteRouteStatus : std::uint8_t {
    no_map_cells,
    cached_cells_updated,
    viewport_draw_queued,
    unresolved_cell_data
};

struct ObjectSpriteRouteBatch {
    ObjectSpriteRouteStatus status{ObjectSpriteRouteStatus::no_map_cells};
    std::vector<MapTileRenderBatch> tile_batches;
    std::optional<TileDrawRequest> viewport_draw;
};

using TileRenderCellProvider = std::function<TileRenderCell&(
    std::int32_t local_x, std::int32_t local_y)>;
struct TileRenderCellCandidate {
    TileRenderCell* cell{};
    std::optional<std::int32_t> filter_value;
};

using TileRenderCellLookup = std::function<TileRenderCellCandidate(
    std::int32_t local_x, std::int32_t local_y)>;

[[nodiscard]] TileRenderCellLookup make_filtered_tile_render_cell_lookup(
    const MapRenderFilterGrid& filter_grid,
    const TileRenderCellProvider& cell_provider);

[[nodiscard]] MapCacheCoordinate map_cache_coordinate(
    std::int32_t local_x, std::int32_t local_y,
    std::int32_t ring_offset_x, std::int32_t ring_offset_y);

[[nodiscard]] MapCacheCoordinate map_cache_coordinate(
    std::int32_t local_x, std::int32_t local_y,
    const DecodedMap& map);

[[nodiscard]] TileRenderBatch queue_visible_tile(
    std::int32_t local_x, std::int32_t local_y,
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    TileRenderCell& cell);

[[nodiscard]] TileObjectUpdateBatch add_tile_render_object(
    std::int32_t local_x, std::int32_t local_y,
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    TileRenderCell& cell, std::int32_t selector,
    std::int32_t offset_x, std::int32_t offset_y);

[[nodiscard]] ObjectSpriteRouteBatch route_object_sprite(
    std::int32_t screen_x, std::int32_t screen_y,
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    std::int32_t sprite_id, bool bypass_tile_cache,
    std::int32_t maximum_cell_value,
    const TileRenderCellLookup& cell_lookup);

[[nodiscard]] std::vector<MapTileRenderBatch> queue_visible_map_tiles(
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    const TileRenderCellProvider& cell_provider);

struct MapEffectTarget {
    std::int32_t local_x{};
    std::int32_t local_y{};
    std::int32_t offset_x{};
    std::int32_t offset_y{};
};

enum class MapEffectPath : std::uint8_t {
    none,
    direct,
    extended,
    tile_overlays,
    unresolved
};

struct MapEffectProjection {
    MapEffectPath path{MapEffectPath::none};
    std::int32_t screen_x{};
    std::int32_t screen_y{};
    std::int32_t texture{};
    std::int32_t extent{};
    std::int32_t extra{};
    std::vector<MapEffectTarget> targets;
};

[[nodiscard]] MapEffectProjection project_map_effect(
    const LightField& field, std::int32_t screen_x, std::int32_t screen_y,
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    std::int32_t maximum_field_value, std::int32_t texture,
    bool partial_effect, std::int32_t extent, std::int32_t extra);

}
