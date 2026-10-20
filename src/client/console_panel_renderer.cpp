#include "client/console_panel_renderer.hpp"
#include "client/ui_skins_792.hpp"

namespace tibia::client {

std::optional<std::array<ConsolePanelControlVisual, 7>>
resolve_console_panel_control_visuals(const ConsolePanelLayout& layout,
    std::int32_t console_x, std::int32_t console_y, bool visible,
    bool server_messages_enabled, std::uint8_t volume_state) noexcept {
    if (!visible) return std::nullopt;
    const auto make_control = [console_x, console_y](ConsolePanelPoint point,
        std::int32_t width, std::int32_t height, std::uint16_t skin,
        std::uint16_t hover_skin, std::uint16_t pressed_skin) {
        return ConsolePanelControlVisual{
            {console_x + point.x, console_y + point.y, width, height}, skin,
            hover_skin, pressed_skin};
    };
    constexpr std::array<std::uint16_t, 3> volume_skins{197, 201, 199};
    constexpr std::array<std::uint16_t, 3> volume_pressed_skins{198, 202, 200};
    const auto volume_index = static_cast<std::size_t>(volume_state % volume_skins.size());
    const auto volume_skin = volume_skins[volume_index];
    return std::array<ConsolePanelControlVisual, 7>{
        make_control(layout.close_channel, 16, 16, 188, 188, 189),
        make_control(layout.server_messages, 16, 16,
            static_cast<std::uint16_t>(server_messages_enabled ? 191 : 190),
            static_cast<std::uint16_t>(server_messages_enabled ? 191 : 190),
            static_cast<std::uint16_t>(server_messages_enabled ? 191 : 190)),
        make_control(layout.ignore_player, 16, 16, 192, 192, 194),
        make_control(layout.open_channel, 16, 16, 195, 195, 196),
        make_control(layout.move_channels_right, 18, 18, 203, 205, 206),
        make_control(layout.move_channels_left, 18, 18, 207, 209, 210),
        make_control(layout.volume_control, 16, 16, volume_skin, volume_skin,
            volume_pressed_skins[volume_index])};
}

std::optional<std::array<ConsolePanelSkinVisual, 8>>
resolve_console_message_frame_visuals(const ConsolePanelLayout& layout,
    std::int32_t console_x, std::int32_t console_y, bool visible) noexcept {
    if (!visible || layout.message_frame.width <= 0 ||
        layout.message_frame.height <= 0)
        return std::nullopt;
    const auto& frame = layout.message_frame;
    const auto& top_left = ui_skins[169];
    const auto& top_right = ui_skins[170];
    const auto& bottom_left = ui_skins[171];
    const auto& bottom_right = ui_skins[172];
    const auto& left_edge = ui_skins[173];
    const auto& right_edge = ui_skins[174];
    const auto& top_edge = ui_skins[175];
    const auto& bottom_edge = ui_skins[176];
    const auto left = left_edge.width;
    const auto right = right_edge.width;
    const auto top = top_left.height;
    const auto bottom = bottom_left.height;
    const auto width = frame.width;
    const auto height = frame.height;
    if (width < left + right || height < top + bottom)
        return std::nullopt;
    const auto x = console_x + frame.x;
    const auto y = console_y + frame.y;
    return std::array<ConsolePanelSkinVisual, 8>{
        ConsolePanelSkinVisual{{x, y, top_left.width, top_left.height}, 169},
        ConsolePanelSkinVisual{{x + left, y, width - left - right,
            top_edge.height}, 175},
        ConsolePanelSkinVisual{{x + width - right, y, top_right.width,
            top_right.height}, 170},
        ConsolePanelSkinVisual{{x, y + top, left_edge.width,
            height - top - bottom}, 173},
        ConsolePanelSkinVisual{{x + width - right, y + top, right_edge.width,
            height - top - bottom}, 174},
        ConsolePanelSkinVisual{{x, y + height - bottom, bottom_left.width,
            bottom_left.height}, 171},
        ConsolePanelSkinVisual{{x + left, y + height - bottom,
            width - left - right, bottom_edge.height}, 176},
        ConsolePanelSkinVisual{{x + width - right, y + height - bottom,
            bottom_right.width, bottom_right.height}, 172}};
}

std::optional<ConsoleTextInputVisual> resolve_console_text_input_visual(
    const ConsolePanelLayout& layout, std::int32_t console_x,
    std::int32_t console_y, bool visible, bool focused) noexcept {
    if (!visible || layout.text_input.width <= 0 ||
        layout.text_input.height <= 0)
        return std::nullopt;
    const ConsolePanelRect rect{
        console_x + layout.text_input.x,
        console_y + layout.text_input.y,
        layout.text_input.width,
        layout.text_input.height};
    return ConsoleTextInputVisual{rect, 4, 224,
        static_cast<std::uint8_t>(focused ? 71 : 54), 3, 3, 255, focused};
}

std::optional<ConsoleTabVisual> resolve_report_violation_tab_visual(
    const ConsolePanelLayout& layout, std::int32_t console_x,
    std::int32_t console_y, bool visible, bool active,
    bool rightmost) noexcept {
    constexpr std::int32_t tab_width = 96;
    constexpr std::int32_t tab_height = 18;
    const auto x_offset = rightmost ? tab_width : 0;
    if (!visible || layout.tab_strip.width < x_offset + tab_width ||
        layout.tab_strip.height < tab_height)
        return std::nullopt;
    const ConsolePanelRect rect{
        console_x + layout.tab_strip.x + x_offset,
        console_y + layout.tab_strip.y,
        tab_width,
        tab_height};
    return ConsoleTabVisual{rect, L"Report Rule Violation",
        static_cast<std::uint16_t>(active ? 186 : 187), 48, 4, 1,
        static_cast<std::uint8_t>(active ? 224 : 128)};
}

std::optional<ConsoleTabVisual> resolve_default_console_tab_visual(
    const ConsolePanelLayout& layout, std::int32_t console_x,
    std::int32_t console_y, bool active, bool rightmost) noexcept {
    constexpr std::int32_t tab_width = 96;
    constexpr std::int32_t tab_height = 18;
    const auto x_offset = rightmost ? tab_width : 0;
    if (layout.tab_strip.width < x_offset + tab_width ||
        layout.tab_strip.height < tab_height)
        return std::nullopt;
    const ConsolePanelRect rect{
        console_x + layout.tab_strip.x + x_offset,
        console_y + layout.tab_strip.y,
        tab_width,
        tab_height};
    return ConsoleTabVisual{rect, L"Default",
        static_cast<std::uint16_t>(active ? 186 : 187), 48, 4, 1,
        static_cast<std::uint8_t>(active ? 224 : 128)};
}

}
