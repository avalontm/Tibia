#pragma once

#include "client/automap_panel_layout.hpp"
#include "client/player_status_bars_layout.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace tibia::client {

inline constexpr int inventory_panel_width = 108;
inline constexpr int inventory_panel_height = 163;
inline constexpr int inventory_content_width = 176;
inline constexpr int inventory_content_height = 170;
inline constexpr int inventory_panel_offset_x = 8;
inline constexpr int inventory_panel_offset_y =
    automap_panel_height + player_status_window_height;
inline constexpr int inventory_minimize_button_x = 8;
inline constexpr int inventory_minimize_button_y = inventory_panel_offset_y;
inline constexpr int inventory_maximize_button_x = 8;
inline constexpr int inventory_maximize_button_y = inventory_panel_offset_y;
inline constexpr int inventory_minimized_content_x = 22;
inline constexpr int inventory_minimized_content_y = inventory_panel_offset_y;
inline constexpr int inventory_minimized_content_width = 32;
inline constexpr int inventory_minimized_content_height = 42;
inline constexpr int inventory_minimized_window_height = 64;
inline constexpr int inventory_window_button_size = 12;
inline constexpr int inventory_item_slot_size = 32;

struct InventorySlotLayout {
    std::uint8_t slot{};
    int x{};
    int y{};
    std::uint16_t empty_object_type{};
    int frame_x{};
    int frame_y{};
};

inline constexpr std::array<InventorySlotLayout, 10> inventory_slot_layouts{{
    {1, 38, 1, 0x58, 37, 0},
    {2, 1, 15, 0x59, 0, 14},
    {3, 75, 15, 0x5a, 74, 14},
    {4, 38, 38, 0x5b, 37, 37},
    {5, 75, 52, 0x5d, 74, 51},
    {6, 1, 52, 0x5c, 0, 51},
    {7, 38, 75, 0x5e, 37, 74},
    {8, 75, 112, 0x5f, 74, 111},
    {9, 1, 89, 0x60, 0, 88},
    {10, 75, 89, 0x61, 74, 88}
}};

[[nodiscard]] std::optional<std::uint8_t> inventory_slot_at(
    int panel_x, int panel_y, int cursor_x, int cursor_y) noexcept;

}
