#pragma once

#include "client/map_window_layout.hpp"

#include <array>
#include <cstdint>

namespace tibia::client {

struct MapWindowFrameEdge {
    std::uint16_t skin{};
    MapWindowLayout rect;
};

struct MapWindowFrameLayout {
    std::array<MapWindowFrameEdge, 4> edges;
};

[[nodiscard]] MapWindowFrameLayout layout_map_window_frame(
    const MapWindowLayout& viewport) noexcept;

}
