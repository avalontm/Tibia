#pragma once

#include "client/content_window_frame_layout.hpp"
#include "network/message_dispatcher.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tibia::client {

struct ContainerItemCell {
    ContentWindowRect bounds;
    std::size_t item_index{};
};

struct ContainerPanelLayout {
    std::uint8_t id{};
    ContentWindowRect window;
    ContentWindowFrameLayout frame;
    ContentWindowRect higher_button;
    ContentWindowRect minimize_button;
    ContentWindowRect close_button;
    ContentWindowRect resize_grip;
    ContentWindowRect resize_hit_area;
    int maximum_height{};
    ContentWindowRect scroll_track;
    ContentWindowRect scroll_thumb;
    std::array<ContentWindowRect, 2> scroll_buttons;
    bool scrollable{};
    bool minimized{};
    int visible_rows{};
    int scroll_value{};
    std::vector<ContainerItemCell> item_cells;
};

struct ContainerPanelState {
    std::uint8_t id{};
    int height{};
    int restore_height{};
    int scroll_value{};
    int position_y{-1};
    std::uint32_t z_order{};
    bool minimized{};
};

inline constexpr int container_panel_width = 176;
inline constexpr int container_item_size = 32;
inline constexpr int container_item_step = 34;
inline constexpr int container_column_count = 4;
inline constexpr int container_visible_row_count = 4;
inline constexpr int container_panel_minimum_height =
    content_window_frame_header_height + 0x26;
inline constexpr int container_panel_minimized_height =
    content_window_frame_header_height;
inline constexpr int container_panel_resize_grip_size = 13;
inline constexpr int container_panel_resize_grip_left_offset = 3;
inline constexpr int container_panel_resize_grip_bottom_offset = 16;
inline constexpr int container_panel_resize_hit_area_height = 4;

[[nodiscard]] std::vector<ContainerPanelLayout> layout_container_panels(
    const ContentWindowRect& sidebar,
    int origin_y,
    const std::vector<network::State6Container>& containers,
    std::span<const ContainerPanelState> states);

}
