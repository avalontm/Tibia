#include "client/map_window_frame_layout.hpp"

namespace tibia::client {

MapWindowFrameLayout layout_map_window_frame(
    const MapWindowLayout& viewport) noexcept {
    const auto right = viewport.x + viewport.width - 1;
    const auto bottom = viewport.y + viewport.height - 1;
    MapWindowFrameLayout layout{};
    layout.edges[0] = {33, {viewport.x, viewport.y, viewport.width, 2}};
    layout.edges[1] = {32, {viewport.x, bottom - 1, viewport.width, 2}};
    layout.edges[2] = {31, {viewport.x, viewport.y, 2, viewport.height}};
    layout.edges[3] = {30, {right - 1, viewport.y, 2, viewport.height}};
    return layout;
}

}
