#pragma once

#include "client/console_panel_layout.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace tibia::client {

struct ConsoleTabVisual {
    ConsolePanelRect rect;
    std::wstring_view title;
    std::uint16_t skin{};
    std::int32_t text_center_x{};
    std::int32_t text_y{};
    std::size_t font_index{};
    std::uint8_t tint{};
};

struct ConsolePanelControlVisual {
    ConsolePanelRect rect;
    std::uint16_t skin{};
    std::uint16_t hover_skin{};
    std::uint16_t pressed_skin{};
};

struct ConsolePanelSkinVisual {
    ConsolePanelRect rect;
    std::uint16_t skin{};
};

struct ConsoleTextInputVisual {
    ConsolePanelRect rect;
    std::size_t font_index{};
    std::uint8_t text_tint{};
    std::uint8_t background_tint{};
    std::int32_t inset{};
    std::int32_t text_y_offset{};
    std::size_t max_length{};
    bool focused{};
};

[[nodiscard]] std::optional<std::array<ConsolePanelControlVisual, 7>>
resolve_console_panel_control_visuals(const ConsolePanelLayout& layout,
    std::int32_t console_x, std::int32_t console_y, bool visible,
    bool server_messages_enabled = false, std::uint8_t volume_state = 0) noexcept;

[[nodiscard]] std::optional<std::array<ConsolePanelSkinVisual, 8>>
resolve_console_message_frame_visuals(const ConsolePanelLayout& layout,
    std::int32_t console_x, std::int32_t console_y, bool visible) noexcept;

[[nodiscard]] std::optional<ConsoleTextInputVisual>
resolve_console_text_input_visual(const ConsolePanelLayout& layout,
    std::int32_t console_x, std::int32_t console_y, bool visible,
    bool focused) noexcept;

[[nodiscard]] std::optional<ConsoleTabVisual> resolve_report_violation_tab_visual(
    const ConsolePanelLayout& layout, std::int32_t console_x,
    std::int32_t console_y, bool visible, bool active = true,
    bool rightmost = false) noexcept;

[[nodiscard]] std::optional<ConsoleTabVisual> resolve_default_console_tab_visual(
    const ConsolePanelLayout& layout, std::int32_t console_x,
    std::int32_t console_y, bool active, bool rightmost = false) noexcept;

}
