#include "client/container_panel_layout.hpp"

#include <algorithm>
#include <utility>

namespace tibia::client {
namespace {

constexpr int title_height = content_window_frame_top_height;
constexpr int item_origin_x = 4;
constexpr int item_origin_y = title_height;
constexpr int item_area_width = container_column_count * container_item_step;
constexpr int scrollbar_x = item_origin_x + item_area_width + 4;
constexpr int scrollbar_width = 12;
constexpr int button_size = 12;
constexpr int window_bottom_padding = content_window_frame_bottom_height;

const ContainerPanelState* state_for(std::span<const ContainerPanelState> states,
    std::uint8_t id) {
    const auto found = std::find_if(states.begin(), states.end(), [id](const auto& state) {
        return state.id == id;
    });
    return found == states.end() ? nullptr : &*found;
}

}

std::vector<ContainerPanelLayout> layout_container_panels(
    const ContentWindowRect& sidebar,
    int origin_y,
    const std::vector<network::State6Container>& containers,
    std::span<const ContainerPanelState> states) {
    std::vector<ContainerPanelLayout> result;
    result.reserve(containers.size());
    auto y = origin_y;
    for (const auto& container : containers) {
        const auto content_slots = std::max<std::size_t>(container.capacity,
            container.items.size());
        const auto content_rows = std::max(1, static_cast<int>(
            (content_slots + container_column_count - 1) /
                container_column_count));
        const auto rows = std::min(container_visible_row_count, content_rows);
        const auto natural_height = title_height + rows * container_item_step +
            window_bottom_padding;
        const auto maximum_height = std::max(container_panel_minimum_height,
            title_height + content_rows * container_item_step +
                window_bottom_padding);
        const auto* state = state_for(states, container.id);
        const auto restored_height = state && state->restore_height > 0
            ? state->restore_height : natural_height;
        const auto preferred_height = state && state->height > 0
            ? state->height : restored_height;
        const auto window_y = state && state->position_y >= 0
            ? state->position_y : y;
        const auto window_height = state && state->minimized
            ? container_panel_minimized_height
            : std::clamp(preferred_height, container_panel_minimum_height,
                maximum_height);
        const ContentWindowRect window{sidebar.x, window_y, container_panel_width,
            window_height};
        ContainerPanelLayout panel;
        panel.id = container.id;
        panel.window = window;
        panel.frame = layout_content_window_frame(window);
        panel.higher_button = {window.x + window.width - 39, window.y + 2,
            button_size, button_size};
        panel.minimize_button = {window.x + window.width - 28, window.y + 2,
            button_size, button_size};
        panel.close_button = {window.x + window.width - 15, window.y + 2,
            button_size, button_size};
        panel.resize_grip = {
            window.x + container_panel_resize_grip_left_offset,
            window.y + window.height - container_panel_resize_grip_bottom_offset,
            container_panel_resize_grip_size, container_panel_resize_grip_size};
        panel.resize_hit_area = {window.x,
            window.y + window.height - container_panel_resize_hit_area_height,
            window.width, container_panel_resize_hit_area_height};
        panel.maximum_height = maximum_height;
        panel.minimized = state && state->minimized;
        panel.visible_rows = panel.minimized ? 0 : std::clamp(
            (window_height - title_height - window_bottom_padding) /
                container_item_step, 1, content_rows);
        panel.scroll_value = state ? std::max(0, state->scroll_value) : 0;
        const auto item_rows = static_cast<int>((container.items.size() +
            container_column_count - 1) / container_column_count);
        panel.scrollable = !panel.minimized && item_rows > panel.visible_rows;
        panel.scroll_track = {window.x + scrollbar_x, window.y + item_origin_y + button_size,
            scrollbar_width, std::max(0, panel.visible_rows * container_item_step - button_size * 2)};
        const auto visible_capacity = static_cast<std::size_t>(
            panel.visible_rows * container_column_count);
        panel.scrollable = container.items.size() > visible_capacity;
        const auto thumb_height = panel.scrollable
            ? std::max(button_size, static_cast<int>(
                static_cast<std::size_t>(panel.scroll_track.height) * visible_capacity /
                std::max<std::size_t>(container.items.size(), 1)))
            : 0;
        panel.scroll_thumb = {panel.scroll_track.x, panel.scroll_track.y,
            scrollbar_width, thumb_height};
        const auto maximum_scroll = std::max(0, item_rows - panel.visible_rows);
        panel.scroll_value = std::clamp(panel.scroll_value, 0, maximum_scroll);
        if (maximum_scroll > 0 && panel.scrollable) {
            const auto travel = std::max(0,
                panel.scroll_track.height - panel.scroll_thumb.height);
            panel.scroll_thumb.y += travel * panel.scroll_value / maximum_scroll;
        }
        panel.scroll_buttons = {{
            {window.x + scrollbar_x, window.y + item_origin_y, button_size, button_size},
            {window.x + scrollbar_x, window.y + item_origin_y + panel.visible_rows * container_item_step - button_size,
                button_size, button_size}
        }};
        const auto first_item = static_cast<std::size_t>(panel.scroll_value) *
            container_column_count;
        const auto visible_items = panel.minimized ? 0 : std::min<std::size_t>(
            container.items.size() > first_item ? container.items.size() - first_item : 0,
            static_cast<std::size_t>(panel.visible_rows * container_column_count));
        panel.item_cells.reserve(visible_items);
        for (std::size_t index = 0; index < visible_items; ++index) {
            const auto item_index = first_item + index;
            const auto column = static_cast<int>(index % container_column_count);
            const auto row = static_cast<int>(index / container_column_count);
            panel.item_cells.push_back({
                {window.x + item_origin_x + column * container_item_step,
                    window.y + item_origin_y + row * container_item_step,
                    container_item_size, container_item_size}, item_index});
        }
        result.push_back(std::move(panel));
        y += window_height;
    }
    return result;
}

}
