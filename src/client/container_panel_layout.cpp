#include "client/container_panel_layout.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
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

std::size_t item_count_for(
    std::span<const network::State6Container> containers, std::uint8_t id) {
    const auto found = std::find_if(containers.begin(), containers.end(),
        [id](const auto& container) { return container.id == id; });
    return found == containers.end() ? 0 : found->items.size();
}

}

int container_panel_resize_height(
    int current_height, int requested_height, std::size_t item_count) noexcept {
    const auto rows = item_count / container_column_count +
        (item_count % container_column_count != 0);
    const auto content_maximum = static_cast<std::int64_t>(
        container_scroll_content_base_height) +
        static_cast<std::int64_t>(rows) * container_scroll_content_row_step;
    const auto maximum = static_cast<int>(std::min<std::int64_t>(content_maximum,
        std::numeric_limits<int>::max()));
    if (maximum < requested_height && current_height < requested_height)
        return std::min(current_height, maximum);
    return requested_height;
}

std::vector<ContainerPanelLayout> layout_container_panels(
    const ContentWindowRect& sidebar,
    int origin_y,
    const std::vector<network::State6Container>& containers,
    std::span<const ContainerPanelState> states) {
    std::vector<ContainerPanelLayout> result;
    result.reserve(containers.size());
    std::vector<const network::State6Container*> ordered_containers;
    ordered_containers.reserve(containers.size());
    for (const auto& state : states) {
        if (!state.attached) continue;
        const auto found = std::find_if(containers.begin(), containers.end(),
            [&state](const auto& container) { return container.id == state.id; });
        if (found != containers.end()) ordered_containers.push_back(&*found);
    }
    for (const auto& container : containers) {
        const auto* state = state_for(states, container.id);
        if (state && !state->attached) continue;
        if (std::none_of(ordered_containers.begin(), ordered_containers.end(),
                [&container](const auto* ordered) { return ordered->id == container.id; }))
            ordered_containers.push_back(&container);
    }
    auto y = origin_y;
    for (const auto* container_pointer : ordered_containers) {
        const auto& container = *container_pointer;
        const auto capacity_rows = std::max(1, static_cast<int>(
            (container.capacity + container_column_count - 1) /
                container_column_count));
        const auto rows = std::min(container_visible_row_count, capacity_rows);
        const auto natural_height = title_height + rows * container_item_step +
            window_bottom_padding;
        const auto* state = state_for(states, container.id);
        if (state && !state->attached) continue;
        const auto restored_height = state && state->restore_height > 0
            ? state->restore_height : natural_height;
        const auto preferred_height = state && state->height > 0
            ? state->height : restored_height;
        const auto window_y = state && state->position_y >= 0
            ? state->position_y : y;
        const auto window_height = state && state->minimized
            ? container_panel_minimized_height
            : std::max(preferred_height, container_panel_minimum_height);
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
        panel.minimized = state && state->minimized;
        const auto viewport_height = panel.minimized ? 0 : std::max(0,
            window_height - title_height - window_bottom_padding);
        panel.visible_rows = panel.minimized ? 0 : std::max(1,
            viewport_height / container_item_step);
        panel.scroll_value = state ? std::max(0, state->scroll_value) : 0;
        panel.has_parent = container.has_parent &&
            !(state && state->higher_navigation_pending);
        panel.item_viewport = {window.x + item_origin_x, window.y + item_origin_y,
            item_area_width, viewport_height};
        panel.scroll_track = {window.x + scrollbar_x, window.y + item_origin_y + button_size,
            scrollbar_width, std::max(0, viewport_height - button_size * 2)};
        const auto item_rows = static_cast<int>((container.items.size() +
            container_column_count - 1) / container_column_count);
        const auto content_height = item_rows > 0
            ? item_rows * container_scroll_content_row_step +
                container_scroll_content_base_height
            : 0;
        const auto maximum_scroll = std::max(0, content_height - viewport_height);
        panel.scroll_range = maximum_scroll;
        panel.scrollable = !panel.minimized && maximum_scroll > 0;
        auto thumb_height = 0;
        if (panel.scrollable) {
            const auto visible_proportion = static_cast<float>(viewport_height) /
                static_cast<float>(content_height);
            const auto proportional_height = static_cast<int>(std::nearbyint(
                static_cast<double>(panel.scroll_track.height) * visible_proportion));
            thumb_height = std::min(panel.scroll_track.height,
                std::max(container_scroll_thumb_minimum_height,
                    proportional_height));
        }
        panel.scroll_thumb = {panel.scroll_track.x, panel.scroll_track.y,
            scrollbar_width, thumb_height};
        panel.scroll_value = std::clamp(panel.scroll_value, 0, maximum_scroll);
        if (maximum_scroll > 0 && panel.scrollable) {
            const auto travel = std::max(0,
                panel.scroll_track.height - panel.scroll_thumb.height);
            panel.scroll_thumb.y += travel * panel.scroll_value / maximum_scroll;
        }
        panel.scroll_buttons = {{
            {window.x + scrollbar_x, window.y + item_origin_y, button_size, button_size},
            {window.x + scrollbar_x, window.y + item_origin_y + viewport_height - button_size,
                button_size, button_size}
        }};
        const auto first_row = static_cast<std::size_t>(panel.scroll_value /
            container_item_step);
        const auto last_row = static_cast<std::size_t>((panel.scroll_value +
            viewport_height + container_item_step - 1) / container_item_step);
        const auto first_item = first_row * container_column_count;
        const auto last_item = std::min(container.items.size(),
            last_row * container_column_count);
        panel.item_cells.reserve(panel.minimized || last_item <= first_item
            ? 0 : last_item - first_item);
        for (std::size_t item_index = first_item;
                !panel.minimized && item_index < last_item; ++item_index) {
            const auto column = static_cast<int>(item_index % container_column_count);
            const auto row = static_cast<int>(item_index / container_column_count);
            panel.item_cells.push_back({
                {window.x + item_origin_x + column * container_item_step,
                    window.y + item_origin_y + row * container_item_step -
                        panel.scroll_value,
                    container_item_size, container_item_size}, item_index});
        }
        result.push_back(std::move(panel));
        y += window_height;
    }
    return result;
}

int container_panel_scroll_after_thumb_drag(
    int pointer_y,
    int grab_offset,
    const ContentWindowRect& track,
    int thumb_height,
    int maximum_scroll) noexcept {
    const auto denominator = static_cast<std::int64_t>(track.height) -
        thumb_height + 1;
    if (maximum_scroll <= 0 || denominator <= 0) return 0;
    const auto relative = static_cast<std::int64_t>(pointer_y) - grab_offset - track.y;
    const auto value = relative * (static_cast<std::int64_t>(maximum_scroll) + 1) /
        denominator;
    return static_cast<int>(std::clamp<std::int64_t>(value, 0, maximum_scroll));
}

int container_panel_move_y(
    int requested_y,
    int group_origin_y,
    int group_height,
    int panel_height) noexcept {
    auto relative_y = requested_y - group_origin_y;
    if (relative_y < 0) relative_y = 0;
    const auto maximum_relative_y = group_height - panel_height;
    if (relative_y > maximum_relative_y) relative_y = maximum_relative_y;
    return group_origin_y + relative_y;
}

void adjust_container_panel_group_after_resize(
    std::span<ContainerPanelState> states,
    std::span<const network::State6Container> containers,
    std::uint8_t resized_id,
    int previous_height,
    int new_height,
    int available_height) {
    const auto resized = std::find_if(states.begin(), states.end(),
        [resized_id](const auto& state) { return state.id == resized_id; });
    if (resized == states.end()) return;
    resized->height = std::max(0, new_height);
    const auto height_delta = new_height - previous_height;
    if (height_delta > 0) {
        auto occupied = 0;
        for (const auto& state : states) {
            if (state.attached) occupied += std::max(0, state.height);
        }
        auto overflow = std::max(0, occupied - available_height);
        for (auto& state : states) {
            if (!state.attached || overflow <= 0) continue;
            const auto minimum = state.minimized
                ? container_panel_minimized_height : container_panel_minimum_height;
            const auto reduction = std::min(overflow,
                std::max(0, state.height - minimum));
            state.height -= reduction;
            overflow -= reduction;
        }
    } else if (height_delta < 0) {
        auto freed = -static_cast<std::int64_t>(height_delta);
        const auto resized_index = static_cast<std::size_t>(
            std::distance(states.begin(), resized));
        for (std::size_t index = 0; index < resized_index && freed > 0; ++index) {
            auto& sibling = states[index];
            if (!sibling.attached || sibling.minimized) continue;
            const auto previous_sibling_height = sibling.height;
            const auto requested_height = static_cast<int>(std::min<std::int64_t>(
                std::numeric_limits<int>::max(),
                static_cast<std::int64_t>(previous_sibling_height) + freed));
            const auto minimum = container_panel_minimum_height;
            const auto new_height = std::max(minimum,
                container_panel_resize_height(previous_sibling_height,
                    requested_height,
                    item_count_for(containers, sibling.id)));
            sibling.height = new_height;
            freed += previous_sibling_height - new_height;
        }
    }
    for (auto& state : states) state.position_y = -1;
}

void fit_container_panel_group_for_open(
    std::span<ContainerPanelState> states,
    std::uint8_t opened_id,
    int requested_height,
    int available_height) {
    const auto opened = std::find_if(states.begin(), states.end(),
        [opened_id](const auto& state) { return state.id == opened_id; });
    if (opened == states.end()) return;
    std::int64_t free_height = available_height;
    for (const auto& state : states) {
        if (state.attached) free_height -= std::max(0, state.height);
    }
    if (free_height < requested_height) {
        const auto opened_minimum = opened->minimized
            ? container_panel_minimized_height : container_panel_minimum_height;
        const auto target_free = std::max<std::int64_t>(free_height,
            opened_minimum);
        for (auto& state : states) {
            if (!state.attached || free_height >= target_free) continue;
            const auto old_height = std::max(0, state.height);
            const auto requested_child_height = static_cast<std::int64_t>(old_height) -
                target_free + free_height;
            const auto minimum = state.minimized
                ? container_panel_minimized_height : container_panel_minimum_height;
            state.height = static_cast<int>(std::max<std::int64_t>(minimum,
                requested_child_height));
            free_height += old_height - state.height;
            if (free_height < target_free) {
                state.attached = false;
                state.position_y = -1;
                free_height += state.height;
            }
        }
    }
    opened->height = static_cast<int>(std::max<std::int64_t>(
        opened->minimized ? container_panel_minimized_height
            : container_panel_minimum_height,
        std::min<std::int64_t>(free_height, requested_height)));
    opened->restore_height = opened->height;
    for (auto& state : states) state.position_y = -1;
}

void fit_container_panel_group_after_close(
    std::span<ContainerPanelState> states,
    std::span<const network::State6Container> containers,
    int available_height) {
    auto free_height = available_height;
    for (auto& state : states) {
        if (!state.attached) continue;
        const auto current_height = std::max(0, state.height);
        if (free_height >= current_height) {
            free_height -= current_height;
            continue;
        }
        const auto minimum = state.minimized
            ? container_panel_minimized_height : container_panel_minimum_height;
        if (free_height >= minimum) {
            const auto resized_height = std::max(minimum,
                container_panel_resize_height(current_height, free_height,
                    item_count_for(containers, state.id)));
            state.height = resized_height;
            if (resized_height <= free_height) free_height -= resized_height;
        } else {
            state.attached = false;
        }
    }
    for (auto& state : states) state.position_y = -1;
}

void fit_container_panel_group_to_available_height(
    std::span<ContainerPanelState> states,
    int available_height) {
    auto overflow = -std::max(0, available_height);
    for (const auto& state : states) {
        if (state.attached) overflow += std::max(0, state.height);
    }
    for (auto& state : states) {
        if (!state.attached || overflow <= 0) continue;
        const auto minimum = state.minimized
            ? container_panel_minimized_height : container_panel_minimum_height;
        const auto reduction = std::min(overflow,
            std::max(0, state.height - minimum));
        state.height -= reduction;
        overflow -= reduction;
    }
}

void reorder_container_panel_after_move(
    std::span<ContainerPanelState> states,
    std::uint8_t moved_id,
    int moved_y,
    int group_origin_y) {
    std::vector<std::size_t> visible;
    visible.reserve(states.size());
    for (std::size_t index = 0; index < states.size(); ++index) {
        if (states[index].attached) visible.push_back(index);
    }
    const auto found = std::find_if(visible.begin(), visible.end(),
        [&states, moved_id](const auto index) {
            return states[index].id == moved_id;
        });
    if (found == visible.end()) return;
    const auto position = static_cast<std::size_t>(found - visible.begin());
    int stack_y = group_origin_y;
    for (std::size_t index = 0; index < position; ++index)
        stack_y += std::max(0, states[visible[index]].height);
    const auto moved_height = std::max(0, states[visible[position]].height);
    const auto move_bottom = moved_y + moved_height;
    const auto previous_index = position > 0 ? visible[position - 1] : states.size();
    const auto next_index = position + 1 < visible.size()
        ? visible[position + 1] : states.size();
    const auto next_id = next_index < states.size()
        ? states[next_index].id : std::uint8_t{};
    const bool swap_previous = previous_index < states.size() &&
        moved_y < stack_y - std::max(0, states[previous_index].height) +
            std::max(0, states[previous_index].height) / 2;
    const bool swap_next = next_index < states.size() &&
        stack_y + moved_height + std::max(0, states[next_index].height) / 2 <
            move_bottom;
    if (swap_previous)
        std::iter_swap(states.begin() + previous_index,
            states.begin() + visible[position]);
    if (swap_next) {
        const auto moved = std::find_if(states.begin(), states.end(),
            [moved_id](const auto& state) { return state.id == moved_id; });
        const auto next = std::find_if(states.begin(), states.end(),
            [next_id](const auto& state) { return state.id == next_id; });
        if (moved != states.end() && next != states.end())
            std::iter_swap(moved, next);
    }
}

}
