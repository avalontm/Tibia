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
    ContentWindowRect scroll_track;
    ContentWindowRect scroll_thumb;
    ContentWindowRect item_viewport;
    std::array<ContentWindowRect, 2> scroll_buttons;
    bool scrollable{};
    bool has_parent{};
    bool minimized{};
    int visible_rows{};
    int scroll_value{};
    int scroll_range{};
    std::vector<ContainerItemCell> item_cells;
};

struct ContainerPanelState {
    std::uint8_t id{};
    std::uint64_t observed_container_revision{};
    int height{};
    int restore_height{};
    int scroll_value{};
    int position_y{-1};
    std::uint32_t z_order{};
    bool minimized{};
    bool attached{true};
    bool higher_navigation_pending{};
};

inline constexpr int container_panel_width = 176;
inline constexpr int container_item_size = 32;
inline constexpr int container_item_step = 34;
inline constexpr int container_scroll_content_row_step = 37;
inline constexpr int container_scroll_content_base_height = 7;
inline constexpr int container_scroll_thumb_cap_height = 6;
inline constexpr int container_scroll_thumb_minimum_height =
    container_scroll_thumb_cap_height * 2;
inline constexpr int container_scroll_step = 10;
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

[[nodiscard]] int container_panel_resize_height(
    int current_height, int requested_height, std::size_t item_count) noexcept;

[[nodiscard]] std::vector<ContainerPanelLayout> layout_container_panels(
    const ContentWindowRect& sidebar,
    int origin_y,
    const std::vector<network::State6Container>& containers,
    std::span<const ContainerPanelState> states);

[[nodiscard]] int container_panel_scroll_after_thumb_drag(
    int pointer_y,
    int grab_offset,
    const ContentWindowRect& track,
    int thumb_height,
    int maximum_scroll) noexcept;

[[nodiscard]] int container_panel_move_y(
    int requested_y,
    int group_origin_y,
    int group_height,
    int panel_height) noexcept;

void adjust_container_panel_group_after_resize(
    std::span<ContainerPanelState> states,
    std::span<const network::State6Container> containers,
    std::uint8_t resized_id,
    int previous_height,
    int new_height,
    int available_height);

void fit_container_panel_group_for_open(
    std::span<ContainerPanelState> states,
    std::uint8_t opened_id,
    int requested_height,
    int available_height);

void fit_container_panel_group_after_close(
    std::span<ContainerPanelState> states,
    std::span<const network::State6Container> containers,
    int available_height);

void fit_container_panel_group_to_available_height(
    std::span<ContainerPanelState> states,
    int available_height);

void reorder_container_panel_after_move(
    std::span<ContainerPanelState> states,
    std::uint8_t moved_id,
    int moved_y,
    int group_origin_y);

}
