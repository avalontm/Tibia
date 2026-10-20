#pragma once

#include "map/light_field.hpp"

#include <vector>

namespace tibia::client {

struct MapWindowItemFrame;
struct GameWorldSnapshot;
}

namespace tibia::resources {
struct TibiaDatFile;
}

namespace tibia::client {

[[nodiscard]] std::vector<map::LightSource> collect_map_window_item_light_sources(
    const MapWindowItemFrame& frame, const resources::TibiaDatFile& dat);
[[nodiscard]] std::vector<map::LightSource> collect_map_window_creature_light_sources(
    const GameWorldSnapshot& snapshot, const MapWindowItemFrame& frame,
    const resources::TibiaDatFile& dat);

}
