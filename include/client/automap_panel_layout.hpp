#pragma once

#include "client\map_window_layout.hpp"

#include <array>
#include <cstdint>

namespace tibia::client {

inline constexpr std::int32_t automap_panel_full_inventory_y = 0;
inline constexpr std::int32_t automap_panel_minimized_inventory_y = 0;
inline constexpr std::int32_t automap_control_row_offset = 296;
inline constexpr std::int32_t automap_panel_height = 117;

struct AutomapButtonLayout {
    MapWindowLayout bounds;
    std::uint16_t normal_skin{};
    std::uint16_t pressed_skin{};
};

struct AutomapPanelLayout {
    MapWindowLayout panel;
    MapWindowLayout viewport;
    MapWindowLayout compass;
    std::array<AutomapButtonLayout, 4> controls;
    AutomapButtonLayout centre;
};

[[nodiscard]] AutomapPanelLayout layout_automap_panel(
    std::int32_t sidebar_x, std::int32_t panel_y) noexcept;

}
