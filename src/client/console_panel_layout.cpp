#include "client/console_panel_layout.hpp"
#include "client/ui_skins_792.hpp"

#include <algorithm>
#include <limits>

namespace tibia::client {
namespace {

std::optional<ConsolePanelRect> resolve_child_rect(std::int32_t parent_width,
    std::int32_t parent_height, std::int32_t x, std::int32_t y,
    std::int32_t width, std::int32_t height) noexcept {
    std::int64_t resolved_x = x;
    std::int64_t resolved_y = y;
    if (resolved_x < 0) resolved_x += parent_width;
    if (resolved_y < 0) resolved_y += parent_height;
    std::int64_t resolved_width = width;
    std::int64_t resolved_height = height;
    if (resolved_width < 1) resolved_width = parent_width - resolved_x + resolved_width;
    if (resolved_height < 1) resolved_height = parent_height - resolved_y + resolved_height;
    if (resolved_x < std::numeric_limits<std::int32_t>::min() ||
        resolved_x > std::numeric_limits<std::int32_t>::max() ||
        resolved_y < std::numeric_limits<std::int32_t>::min() ||
        resolved_y > std::numeric_limits<std::int32_t>::max() ||
        resolved_width <= 0 || resolved_width > std::numeric_limits<std::int32_t>::max() ||
        resolved_height <= 0 || resolved_height > std::numeric_limits<std::int32_t>::max())
        return std::nullopt;
    return ConsolePanelRect{static_cast<std::int32_t>(resolved_x),
        static_cast<std::int32_t>(resolved_y), static_cast<std::int32_t>(resolved_width),
        static_cast<std::int32_t>(resolved_height)};
}

ConsolePanelPoint resolve_child_point(std::int32_t parent_width,
    std::int32_t parent_height, std::int32_t x, std::int32_t y) noexcept {
    if (x < 0) x += parent_width;
    if (y < 0) y += parent_height;
    return {x, y};
}

}

std::optional<ConsolePanelLayout> resolve_console_panel_layout(
    std::int32_t width, std::int32_t height) noexcept {
    if (width <= 0 || height <= 18) return std::nullopt;
    const auto tab_strip = resolve_child_rect(width, height, 18, 0, -82, 18);
    const auto message_frame = resolve_child_rect(width, height, 4, 21, -4, -22);
    const auto text_input = resolve_child_rect(width, height, 23, -20, -4, 16);
    if (!tab_strip || !text_input || !message_frame) return std::nullopt;
    const auto message_view = resolve_child_rect(message_frame->width,
        message_frame->height, 5, 5, -17, -5);
    const auto message_scrollbar = resolve_child_rect(message_frame->width,
        message_frame->height, -15, 3, 12, -3);
    if (!message_view || !message_scrollbar) return std::nullopt;
    const ConsolePanelRect root_message_view{
        message_frame->x + message_view->x,
        message_frame->y + message_view->y,
        message_view->width,
        message_view->height};
    const ConsolePanelRect root_message_scrollbar{
        message_frame->x + message_scrollbar->x,
        message_frame->y + message_scrollbar->y,
        message_scrollbar->width,
        message_scrollbar->height};
    return ConsolePanelLayout{
        *tab_strip,
        *message_frame,
        *text_input,
        root_message_view,
        root_message_scrollbar,
        resolve_child_point(width, height, 5, -20),
        resolve_child_point(width, height, -64, 0),
        resolve_child_point(width, height, -48, 0),
        resolve_child_point(width, height, -16, 0),
        resolve_child_point(width, height, -32, 0),
        resolve_child_point(width, height, 0, 0),
        resolve_child_point(width, height, -82, 0)};
}

ConsoleMessageScrollbarGeometry resolve_console_message_scrollbar_geometry(
    std::int32_t width, std::int32_t height, std::int32_t item_count,
    std::int32_t visible_items, std::int32_t first_visible) noexcept {
    ConsoleMessageScrollbarGeometry geometry;
    if (width <= 0 || height <= 0) return geometry;
    const auto arrow_height = std::min(12, height / 2);
    geometry.up_arrow = {0, 0, width, arrow_height};
    geometry.down_arrow = {0, height - arrow_height, width, arrow_height};
    const auto track_height = std::max(0, height - 2 * arrow_height);
    geometry.track = {0, arrow_height, width, track_height};
    const auto maximum = std::max(0, item_count - visible_items);
    if (item_count <= 0 || visible_items <= 0 || maximum <= 0 ||
        track_height <= 0)
        return geometry;
    const auto minimum_thumb_height = std::min(track_height,
        ui_skins[154].height + ui_skins[156].height);
    const auto proportional_thumb_height = static_cast<std::int32_t>(
        static_cast<std::int64_t>(track_height) * visible_items / item_count);
    const auto thumb_height = std::clamp(proportional_thumb_height,
        minimum_thumb_height, track_height);
    const auto travel = track_height - thumb_height;
    const auto position = static_cast<std::int32_t>(
        static_cast<std::int64_t>(std::clamp(first_visible, 0, maximum)) * travel /
        maximum);
    geometry.thumb = {0, arrow_height + position, width, thumb_height};
    geometry.thumb_visible = true;
    return geometry;
}

}
