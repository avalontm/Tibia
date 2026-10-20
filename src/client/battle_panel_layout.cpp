#include "client/battle_panel_layout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tibia::client {

BattlePanelLayout layout_battle_panel_rows(
    std::span<const BattleCreatureRow> rows,
    BattlePanelRect viewport, int scroll_offset) {
    BattlePanelLayout layout;
    layout.viewport = viewport;
    const auto bounded_count = std::min(rows.size(),
        static_cast<std::size_t>((std::numeric_limits<int>::max() - 1) /
            battle_panel_row_height));
    layout.content_height = static_cast<int>(bounded_count) *
        battle_panel_row_height + 1;
    const auto visible_height = std::max(0, viewport.height);
    layout.scroll_range = std::max(visible_height, layout.content_height) -
        visible_height;
    layout.scroll_offset = std::clamp(scroll_offset, 0, layout.scroll_range);
    layout.rows.reserve(bounded_count);

    for (std::size_t index = 0; index < bounded_count; ++index) {
        const auto& row = rows[index];
        const auto row_index = static_cast<int>(index);
        const auto top = viewport.y + row_index * battle_panel_row_height -
            layout.scroll_offset;
        const auto bottom = top + battle_panel_row_height;
        const auto clip_top = std::max(top, viewport.y);
        const auto clip_bottom = std::min(bottom, viewport.y + visible_height);
        const auto clip_left = viewport.x;
        const auto clip_right = viewport.x + std::max(0, viewport.width);
        const BattlePanelRect bounds{
            viewport.x, top, std::max(0, viewport.width), battle_panel_row_height};
        const BattlePanelRect clip{clip_left, clip_top, clip_right - clip_left,
            std::max(0, clip_bottom - clip_top)};
        auto text_offset = 0;
        const auto shield = row.party_shield_skin
            ? std::optional<BattlePanelRect>{{viewport.x + 23, top + 2, 0, 0}}
            : std::nullopt;
        if (shield) text_offset += 13;
        const auto skull = row.skull_skin
            ? std::optional<BattlePanelRect>{{viewport.x + 23 + text_offset, top + 2, 0, 0}}
            : std::nullopt;
        if (skull) text_offset += 13;
        const auto health = std::min<std::uint8_t>(row.health_percent, 100);
        layout.rows.push_back({index,
            bounds,
            clip,
            {viewport.x + 2, top + 2, 18, 18},
            {viewport.x + 23 + text_offset, top + 2, 110, 11},
            row.party_shield_skin,
            shield.value_or(BattlePanelRect{}),
            row.skull_skin,
            skull.value_or(BattlePanelRect{}),
            {viewport.x + 23, top + 14, 122, 5},
            {viewport.x + 24, top + 15, static_cast<int>(health) * 120 / 100, 3}
        });
    }
    return layout;
}

BattlePanelScrollbarLayout layout_battle_panel_scrollbar(
    BattlePanelRect bounds, int content_height, int viewport_height,
    int scroll_offset) {
    BattlePanelScrollbarLayout layout;
    if (bounds.width <= 0 || bounds.height <= 0) return layout;

    constexpr int arrow_height = 12;
    constexpr int thumb_cap_height = 6;
    const auto track_height = std::max(0, bounds.height - arrow_height * 2);
    layout.up_arrow = {bounds.x, bounds.y, bounds.width, arrow_height};
    layout.track = {bounds.x, bounds.y + arrow_height, bounds.width, track_height};
    layout.down_arrow = {bounds.x, bounds.y + bounds.height - arrow_height,
        bounds.width, arrow_height};

    const auto page = std::max(0, viewport_height);
    const auto content = std::max(std::max(0, content_height), page);
    const auto maximum = content - page;
    if (maximum <= 0 || page <= 0 || track_height <= 0) return layout;

    const auto proportional_height = static_cast<int>(std::nearbyint(
        static_cast<double>(track_height) * page / content));
    const auto minimum_thumb_height = std::min(2 * thumb_cap_height, track_height);
    const auto thumb_height = std::clamp(proportional_height,
        minimum_thumb_height, track_height);
    const auto travel = track_height - thumb_height;
    const auto offset = std::clamp(scroll_offset, 0, maximum);
    const auto thumb_offset = static_cast<int>(
        static_cast<std::int64_t>(offset) * travel / maximum);
    layout.thumb_visible = true;
    layout.thumb = {bounds.x, layout.track.y + thumb_offset,
        bounds.width, thumb_height};
    return layout;
}

}
