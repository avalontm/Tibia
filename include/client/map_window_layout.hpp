#pragma once

#include "client/console_panel_layout.hpp"

#include <cstdint>
#include <optional>

namespace tibia::client {

inline constexpr std::int32_t map_panel_height = 106;
inline constexpr std::int32_t map_window_right_margin = 176;
inline constexpr std::int32_t map_window_bottom_margin = 5;
inline constexpr std::int32_t map_window_reference_width = 480;
inline constexpr std::int32_t map_window_reference_height = 352;
inline constexpr std::int32_t map_window_minimum_height = 176;

struct MapWindowLayoutInput {
    std::int32_t client_width{};
    std::int32_t client_height{};
    std::int32_t console_height{};
    std::int32_t panel_height{map_panel_height};
    std::int32_t map_window_height{map_window_minimum_height};
    bool no_stretching{};
};

struct MapWindowLayout {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
};

struct GameWindowLayout {
    MapWindowLayout map;
    MapWindowLayout sidebar;
    MapWindowLayout console;
    MapWindowLayout bottom_gap;
    std::optional<ConsolePanelLayout> console_panel;
};

[[nodiscard]] std::optional<MapWindowLayout> resolve_map_window_layout(
    const MapWindowLayoutInput& input) noexcept;
[[nodiscard]] std::optional<GameWindowLayout> resolve_game_window_layout(
    const MapWindowLayoutInput& input) noexcept;

}
