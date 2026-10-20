#pragma once

#include "map/model.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace tibia::resources { struct TibiaDatFile; }

namespace tibia::map {

inline constexpr std::int32_t map_render_maximum_floor = 7;
inline constexpr std::size_t map_render_cell_count = 18U * 14U;

struct MapRenderFilterGrid {
    std::array<std::optional<std::int32_t>, map_render_cell_count> values{};

    [[nodiscard]] std::optional<std::int32_t> value_at(
        std::int32_t local_x, std::int32_t local_y) const noexcept;
};

[[nodiscard]] MapRenderFilterGrid compute_map_render_filter_grid(
    const DecodedMap& map, const resources::TibiaDatFile& dat,
    std::int32_t highest_active_floor);

[[nodiscard]] std::optional<std::int32_t> compute_highest_active_render_floor(
    const DecodedMap& map, const resources::TibiaDatFile& dat);

}
