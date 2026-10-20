#pragma once

#include "client/talk_system.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tibia::client {

enum class TalkboxTextAlignment : std::uint8_t {
    left,
    center,
    right
};

struct TalkboxTextExtent {
    std::int32_t width{};
    std::int32_t height{};
};

struct TalkboxLayoutContext {
    std::int32_t viewport_width{};
    std::int32_t viewport_height{};
    std::int32_t status_reserved_bottom{};
    std::int32_t scroll_offset_pixels_x{};
    std::int32_t scroll_offset_pixels_y{};
    bool pixel_coordinates{};
};

struct TalkboxPlacement {
    std::size_t talkbox_index{};
    std::int32_t x{};
    std::int32_t y{};
    TalkboxTextAlignment alignment{};
    std::uint8_t edge_flags{};
};

struct StatusMessagePlacement {
    std::int32_t anchor_x{};
    std::int32_t baseline_y{};
    TalkboxTextAlignment alignment{TalkboxTextAlignment::center};
    TalkboxColor color{255, 255, 255};
};

struct TalkboxTextDrawCommand {
    std::size_t talkbox_index{};
    std::size_t line_index{};
    std::string text;
    std::int32_t anchor_x{};
    std::int32_t top_y{};
    std::uint8_t font_number{2};
    TalkboxTextAlignment alignment{TalkboxTextAlignment::left};
    TalkboxColor color{};
};

[[nodiscard]] std::optional<StatusMessagePlacement> layout_status_message(
    const TalkSystem& talk_system, std::int32_t viewport_width,
    std::int32_t viewport_height, std::int32_t font_height,
    std::int32_t canvas_offset_x, std::int32_t canvas_offset_y) noexcept;

[[nodiscard]] std::vector<TalkboxTextDrawCommand> build_talkbox_text_commands(
    const std::array<Talkbox, TalkSystem::talkbox_count>& talkboxes,
    const std::vector<TalkboxPlacement>& placements, std::int32_t font_line_height,
    std::int32_t canvas_offset_x, std::int32_t canvas_offset_y);

[[nodiscard]] std::vector<TalkboxPlacement> layout_talkboxes(
    const std::array<Talkbox, TalkSystem::talkbox_count>& talkboxes,
    const std::array<TalkboxTextExtent, TalkSystem::talkbox_count>& text_extents,
    const network::State6DecodedMap& map,
    const TalkboxLayoutContext& context);

}
