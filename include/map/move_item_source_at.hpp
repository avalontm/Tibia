#pragma once

#include "client/game_world_snapshot.hpp"
#include "client/map_window_camera.hpp"
#include "client/map_window_layout.hpp"
#include "map/model.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace tibia::resources {
struct TibiaDatFile;
}

namespace tibia::map {

struct MoveItemSource {
    const Tile* tile{};
    const Object* object{};
    std::size_t stack_position{};
};

[[nodiscard]] std::optional<MoveItemSource> move_item_source_at(
    const client::GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    const client::MapWindowLayout& viewport,
    client::MapWindowCameraOffset camera_offset,
    bool no_stretching,
    std::int32_t click_x,
    std::int32_t click_y);

}
