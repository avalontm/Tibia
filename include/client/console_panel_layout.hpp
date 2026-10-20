#pragma once

#include <cstdint>
#include <optional>

namespace tibia::client {

struct ConsolePanelPoint {
    std::int32_t x{};
    std::int32_t y{};
};

struct ConsolePanelRect {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
};

struct ConsoleMessageScrollbarGeometry {
    ConsolePanelRect up_arrow;
    ConsolePanelRect track;
    ConsolePanelRect thumb;
    ConsolePanelRect down_arrow;
    bool thumb_visible{};
};

struct ConsolePanelLayout {
    ConsolePanelRect tab_strip;
    ConsolePanelRect message_frame;
    ConsolePanelRect text_input;
    ConsolePanelRect message_view;
    ConsolePanelRect message_scrollbar;
    ConsolePanelPoint volume_control;
    ConsolePanelPoint close_channel;
    ConsolePanelPoint server_messages;
    ConsolePanelPoint ignore_player;
    ConsolePanelPoint open_channel;
    ConsolePanelPoint move_channels_right;
    ConsolePanelPoint move_channels_left;
};

[[nodiscard]] std::optional<ConsolePanelLayout> resolve_console_panel_layout(
    std::int32_t width, std::int32_t height) noexcept;
[[nodiscard]] ConsoleMessageScrollbarGeometry resolve_console_message_scrollbar_geometry(
    std::int32_t width, std::int32_t height, std::int32_t item_count,
    std::int32_t visible_items, std::int32_t first_visible) noexcept;

}
