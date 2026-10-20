#include "client/player_status_bars_layout.hpp"

#include <algorithm>
#include <cstdint>

namespace tibia::client {

std::array<PlayerStatusBarLayout, 2> layout_player_status_bars(
    int sidebar_x, int status_window_y) noexcept {
    return {{
        {sidebar_x + 9, status_window_y + 5,
            sidebar_x + 26, status_window_y + 4,
            sidebar_x + 127, status_window_y + 5,
            119, 120, 121},
        {sidebar_x + 9, status_window_y + 18,
            sidebar_x + 26, status_window_y + 17,
            sidebar_x + 127, status_window_y + 18,
            122, 123, 124}
    }};
}

int player_status_bar_fill_width(
    std::uint32_t value, std::uint32_t maximum) noexcept {
    if (maximum == 0 || value >= maximum)
        return player_status_bar_width;
    return std::clamp(static_cast<int>(
        static_cast<std::uint64_t>(player_status_bar_width) * value / maximum),
        0, player_status_bar_width);
}

}
