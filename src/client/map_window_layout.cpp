#include "client/map_window_layout.hpp"

#include <algorithm>
#include <limits>

namespace tibia::client {

std::optional<MapWindowLayout> resolve_map_window_layout(
    const MapWindowLayoutInput& input) noexcept {
    const auto game_layout = resolve_game_window_layout(input);
    if (!game_layout) return std::nullopt;
    return game_layout->map;
}

std::optional<GameWindowLayout> resolve_game_window_layout(
    const MapWindowLayoutInput& input) noexcept {
    auto console_height = std::max(input.panel_height, input.console_height);
    auto reserved_height = std::max(input.panel_height, console_height);
    auto height = static_cast<std::int64_t>(input.client_height) - reserved_height -
        map_window_bottom_margin;
    if (height < input.map_window_height) {
        console_height = input.panel_height;
        reserved_height = input.panel_height;
        height = static_cast<std::int64_t>(input.client_height) - reserved_height -
            map_window_bottom_margin;
    }
    const auto width = static_cast<std::int64_t>(input.client_width) -
        map_window_right_margin;
    if (width < std::numeric_limits<std::int32_t>::min() ||
        width > std::numeric_limits<std::int32_t>::max() ||
        height < std::numeric_limits<std::int32_t>::min() ||
        height > std::numeric_limits<std::int32_t>::max())
        return std::nullopt;
    const auto map_width = static_cast<std::int32_t>(width);
    const auto map_height = static_cast<std::int32_t>(height);
    const auto viewport_width = static_cast<std::int32_t>(input.no_stretching
        ? std::min<std::int64_t>(map_width, map_window_reference_width)
        : map_width);
    const auto viewport_height = static_cast<std::int32_t>(input.no_stretching
        ? std::min<std::int64_t>(map_height, map_window_reference_height)
        : map_height);
    if (viewport_width <= 0 || viewport_height <= 0) return std::nullopt;
    const bool center_fixed_viewport = input.no_stretching &&
        (map_width < map_window_reference_width ||
            map_height < map_window_reference_height);
    const MapWindowLayout map_viewport{
        center_fixed_viewport ? (map_width - viewport_width) / 2 : 0,
        center_fixed_viewport ? (map_height - viewport_height) / 2 : 0,
        viewport_width, viewport_height};
    const auto console_y = static_cast<std::int64_t>(map_height) +
        map_window_bottom_margin;
    if (console_y < std::numeric_limits<std::int32_t>::min() ||
        console_y > std::numeric_limits<std::int32_t>::max())
        return std::nullopt;
    const MapWindowLayout console{0, static_cast<std::int32_t>(console_y),
        map_width, console_height};
    return GameWindowLayout{
        map_viewport,
        {map_width, 0, map_window_right_margin, input.client_height},
        console,
        {0, map_height, map_width, map_window_bottom_margin},
        resolve_console_panel_layout(console.width, console.height)};
}

}
