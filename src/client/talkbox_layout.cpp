#include "client/talkbox_layout.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <stdexcept>

namespace tibia::client {
namespace {

struct WorkingPlacement {
    std::size_t talkbox_index{};
    std::int32_t anchor_x{};
    std::int32_t center_x{};
    std::int32_t center_y{};
    std::int32_t minimum_x{};
    std::int32_t maximum_x{};
    std::int32_t minimum_y{};
    std::int32_t maximum_y{};
    TalkboxTextExtent extent;
    TalkboxTextAlignment alignment{TalkboxTextAlignment::center};
    std::uint8_t edge_flags{};
};

bool uses_screen_anchor(std::uint8_t mode) noexcept {
    return mode == 4 || mode == 9 || mode == 0x0B || mode == 0x12 ||
        mode == 0x13 || mode == 0x16;
}

std::int32_t clamp_axis(std::int32_t value, std::int32_t first,
    std::int32_t second) noexcept {
    if (second < first) {
        if (value < second) value = second;
        if (first < value) value = first;
        return value;
    }
    if (value < first) value = first;
    if (second < value) value = second;
    return value;
}

void add_working_placement(std::vector<WorkingPlacement>& placements,
    std::size_t index, const Talkbox& talkbox, const TalkboxTextExtent& extent,
    const network::State6DecodedMap& map, const TalkboxLayoutContext& context) {
    WorkingPlacement placement;
    placement.talkbox_index = index;
    placement.extent = extent;
    const bool screen_anchor = uses_screen_anchor(talkbox.mode);
    placement.edge_flags = screen_anchor ? 3 : 0;

    if (screen_anchor) {
        placement.anchor_x = context.viewport_width / 2;
        placement.center_x = placement.anchor_x;
        switch (talkbox.mode) {
        case 4:
            placement.center_y = context.viewport_height / 4;
            break;
        case 9:
        case 0x0B:
        case 0x12:
        case 0x13:
            placement.center_y = context.viewport_height / 2 + extent.height / 2 + 5;
            break;
        case 0x16:
            placement.center_y = context.viewport_height / 2 - extent.height / 2 - 5;
            break;
        default:
            throw std::logic_error("talkbox screen-anchor mode is unobserved");
        }
    } else {
        const auto local = network::transform_state6_wire_position(map,
            {talkbox.x, talkbox.y, talkbox.floor});
        const auto offset_x = (480 - context.viewport_width) / 2;
        const auto offset_y = (352 - context.viewport_height) / 2;
        if (context.pixel_coordinates) {
            placement.anchor_x = local.x * 32 - offset_x - 16;
            placement.center_y = local.y * 32 - offset_y - 32 - extent.height / 2;
        } else {
            placement.anchor_x = ((local.x - 1) * context.viewport_width) / 15 +
                context.viewport_width / 30;
            placement.center_y = ((local.y - 1) * context.viewport_height) / 11 -
                extent.height / 2;
        }
        placement.center_x = placement.anchor_x;
    }

    placement.minimum_x = std::max(extent.width / 2 + 1,
        placement.anchor_x - extent.width / 2);
    placement.maximum_x = std::min(context.viewport_width - extent.width / 2,
        placement.anchor_x + extent.width / 2);
    if (screen_anchor) {
        placement.minimum_y = std::max(extent.height / 2 + 1,
            placement.center_y - extent.height);
        placement.maximum_y = std::min(context.viewport_height - extent.height / 2 -
            context.status_reserved_bottom, placement.center_y + extent.height);
    } else {
        placement.minimum_y = std::max(extent.height / 2 + 1, placement.center_y);
        placement.maximum_y = std::min(context.viewport_height - extent.height / 2 -
            context.status_reserved_bottom, placement.center_y + extent.height);
    }
    placement.center_x = clamp_axis(placement.center_x, placement.minimum_x,
        placement.maximum_x);
    placement.center_y = clamp_axis(placement.center_y, placement.minimum_y,
        placement.maximum_y);
    placement.center_x = clamp_axis(placement.center_x, extent.width / 2 + 1,
        context.viewport_width - extent.width / 2);
    placement.center_y = clamp_axis(placement.center_y, extent.height / 2 + 1,
        context.viewport_height - extent.height / 2 - context.status_reserved_bottom);
    placements.push_back(placement);
}

void resolve_overlaps(std::vector<WorkingPlacement>& placements) {
    if (placements.size() < 2) return;
    std::int32_t previous_energy = INT32_MAX;
    std::array<std::int32_t, TalkSystem::talkbox_count> horizontal_forces{};
    std::array<std::int32_t, TalkSystem::talkbox_count> vertical_forces{};

    while (true) {
        const auto energy_limit = previous_energy;
        std::int32_t energy{};
        std::fill(horizontal_forces.begin(), horizontal_forces.end(), 0);
        std::fill(vertical_forces.begin(), vertical_forces.end(), 0);

        for (std::size_t i = 0; i < placements.size(); ++i) {
            for (std::size_t j = 0; j < placements.size(); ++j) {
                if (i == j) continue;
                auto& current = placements[i];
                const auto& other = placements[j];
                const auto delta_x = other.center_x - current.center_x;
                const auto delta_y = other.center_y - current.center_y;
                const auto distance_x = std::abs(delta_x);
                const auto distance_y = std::abs(delta_y);
                const auto half_width_current = current.extent.width / 2;
                const auto half_width_other = other.extent.width / 2;
                const auto combined_half_width = half_width_current + half_width_other;
                if (distance_x >= combined_half_width + 10) continue;

                const auto half_height_current = current.extent.height / 2;
                const auto half_height_other = other.extent.height / 2;
                const auto combined_half_height = half_height_current + half_height_other;
                if (distance_y >= combined_half_height + 10) continue;

                std::int32_t separation_x{};
                if (half_width_other < half_width_current + distance_x) {
                    if (half_width_current < half_width_other + distance_x)
                        separation_x = combined_half_width - distance_x + 10;
                    else
                        separation_x = other.extent.width + 20;
                } else {
                    separation_x = current.extent.width + 20;
                }

                std::int32_t separation_y{};
                if (half_height_other < half_height_current + distance_y) {
                    if (half_height_current < half_height_other + distance_y)
                        separation_y = combined_half_height - distance_y + 10;
                    else
                        separation_y = other.extent.height + 20;
                } else {
                    separation_y = current.extent.height + 20;
                }

                energy += separation_x * separation_y;
                if (delta_x > 0) horizontal_forces[i] -= separation_x;
                if (delta_x < 0) horizontal_forces[i] += separation_x;
                if (delta_y > 0) vertical_forces[i] -= separation_y;
                if (delta_y < 0) vertical_forces[i] += separation_y;
                if (delta_x == 0 && delta_y == 0)
                    horizontal_forces[i] += static_cast<std::int32_t>(i) -
                        static_cast<std::int32_t>(j);
            }
        }

        for (std::size_t i = 0; i < placements.size(); ++i) {
            auto& current = placements[i];
            if (horizontal_forces[i] < 0 && current.minimum_x < current.center_x)
                --current.center_x;
            if (horizontal_forces[i] > 0 && current.center_x < current.maximum_x)
                ++current.center_x;
            if (vertical_forces[i] < 0 && current.minimum_y < current.center_y)
                --current.center_y;
            if (vertical_forces[i] > 0 && current.center_y < current.maximum_y)
                ++current.center_y;
        }
        if (energy >= energy_limit) break;
        previous_energy = energy;
    }
}

}

std::optional<StatusMessagePlacement> layout_status_message(
    const TalkSystem& talk_system, std::int32_t viewport_width,
    std::int32_t viewport_height, std::int32_t font_height,
    std::int32_t canvas_offset_x, std::int32_t canvas_offset_y) noexcept {
    if (!talk_system.has_active_status_message()) return std::nullopt;
    return StatusMessagePlacement{
        viewport_width / 2 + canvas_offset_x,
        viewport_height - font_height + canvas_offset_y,
        TalkboxTextAlignment::center,
        {255, 255, 255}
    };
}

std::vector<TalkboxTextDrawCommand> build_talkbox_text_commands(
    const std::array<Talkbox, TalkSystem::talkbox_count>& talkboxes,
    const std::vector<TalkboxPlacement>& placements, std::int32_t font_line_height,
    std::int32_t canvas_offset_x, std::int32_t canvas_offset_y) {
    std::size_t line_total{};
    for (const auto& placement : placements) {
        if (placement.talkbox_index < talkboxes.size())
            line_total += talkboxes[placement.talkbox_index].lines.size();
    }

    std::vector<TalkboxTextDrawCommand> commands;
    commands.reserve(line_total);
    for (const auto& placement : placements) {
        if (placement.talkbox_index >= talkboxes.size()) continue;
        const auto& talkbox = talkboxes[placement.talkbox_index];
        if (talkbox.state != 1) continue;
        for (std::size_t line_index = 0; line_index < talkbox.lines.size(); ++line_index) {
            commands.push_back({placement.talkbox_index, line_index,
                talkbox.lines[line_index].text, placement.x + canvas_offset_x,
                placement.y + static_cast<std::int32_t>(line_index) * font_line_height +
                    canvas_offset_y,
                2, placement.alignment, talkbox.color});
        }
    }
    return commands;
}

std::vector<TalkboxPlacement> layout_talkboxes(
    const std::array<Talkbox, TalkSystem::talkbox_count>& talkboxes,
    const std::array<TalkboxTextExtent, TalkSystem::talkbox_count>& text_extents,
    const network::State6DecodedMap& map,
    const TalkboxLayoutContext& context) {
    std::vector<WorkingPlacement> placements;
    placements.reserve(TalkSystem::talkbox_count);
    for (std::size_t i = 0; i < talkboxes.size(); ++i) {
        if (talkboxes[i].state != 1) continue;
        add_working_placement(placements, i, talkboxes[i], text_extents[i], map, context);
    }
    resolve_overlaps(placements);

    std::vector<TalkboxPlacement> result;
    result.reserve(placements.size());
    for (auto& placement : placements) {
        const auto width_quarter = placement.extent.width / 4;
        auto x = placement.center_x;
        if (x < placement.anchor_x - width_quarter) {
            placement.alignment = TalkboxTextAlignment::right;
            x += placement.extent.width / 2;
        } else if (x <= placement.anchor_x + width_quarter) {
            placement.alignment = TalkboxTextAlignment::center;
        } else {
            placement.alignment = TalkboxTextAlignment::left;
            x -= placement.extent.width / 2;
        }

        auto flags = placement.edge_flags;
        if (placement.center_x == placement.extent.width / 2 + 1 ||
            placement.center_x == context.viewport_width - placement.extent.width / 2) flags |= 1;
        if (placement.center_y == placement.extent.height / 2 + 1 ||
            placement.center_y == context.viewport_height - placement.extent.height / 2 -
                context.status_reserved_bottom) flags |= 2;
        auto y = placement.center_y - placement.extent.height / 2;
        if ((flags & 1) == 0) {
            const auto scroll_x = context.pixel_coordinates
                ? context.scroll_offset_pixels_x
                : context.scroll_offset_pixels_x * context.viewport_width / 480;
            x -= scroll_x;
        }
        if ((flags & 2) == 0) {
            const auto scroll_y = context.pixel_coordinates
                ? context.scroll_offset_pixels_y
                : context.scroll_offset_pixels_y * context.viewport_height / 352;
            y -= scroll_y;
        }
        result.push_back({placement.talkbox_index, x,
            y,
            placement.alignment, flags});
    }
    return result;
}

}
