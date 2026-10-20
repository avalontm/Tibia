#pragma once

#include <array>
#include <cstdint>

namespace tibia::client {

inline constexpr int player_status_window_height = 32;
inline constexpr int player_status_bar_width = 90;
inline constexpr int player_status_bar_height = 11;

struct PlayerStatusBarLayout {
    int icon_x{};
    int icon_y{};
    int bar_x{};
    int bar_y{};
    int value_x{};
    int value_y{};
    std::uint16_t icon_skin{};
    std::uint16_t track_skin{};
    std::uint16_t fill_skin{};
};

[[nodiscard]] std::array<PlayerStatusBarLayout, 2> layout_player_status_bars(
    int sidebar_x, int status_window_y) noexcept;
[[nodiscard]] int player_status_bar_fill_width(
    std::uint32_t value, std::uint32_t maximum) noexcept;

}
