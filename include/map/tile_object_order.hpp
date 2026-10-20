#pragma once

#include "map/model.hpp"

#include <optional>
#include <vector>

namespace tibia::resources {
struct TibiaDatFile;
}

namespace tibia::map {

[[nodiscard]] std::optional<std::vector<const Object*>>
resolve_native_tile_object_order(const Tile& tile,
    const resources::TibiaDatFile& dat);

[[nodiscard]] bool refresh_native_tile_object_context(Tile& tile,
    const resources::TibiaDatFile& dat) noexcept;

}
