#pragma once

#include "client\map_window_layout.hpp"

#include <array>
#include <cstdint>

namespace tibia::client {

enum class GameScreenControl : std::uint8_t {
    skills,
    battle,
    vip,
    logout
};

struct GameScreenControlButton {
    GameScreenControl control{};
    MapWindowLayout bounds;
    std::uint16_t normal_skin{};
    std::uint16_t pressed_skin{};
};

[[nodiscard]] std::array<GameScreenControlButton, 4>
layout_game_screen_control_buttons(std::int32_t sidebar_x,
    std::int32_t row_y) noexcept;

}
