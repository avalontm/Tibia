#pragma once

#include "client/battle_panel_model.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tibia::client {

inline constexpr int battle_panel_row_height = 22;
inline constexpr int battle_panel_scroll_step = battle_panel_row_height / 3;

struct BattlePanelRect {
    int x{};
    int y{};
    int width{};
    int height{};
};

struct BattlePanelRowLayout {
    std::size_t index{};
    BattlePanelRect bounds;
    BattlePanelRect clip;
    BattlePanelRect creature_cell;
    BattlePanelRect name;
    std::optional<std::uint16_t> party_shield_skin;
    BattlePanelRect party_shield;
    std::optional<std::uint16_t> skull_skin;
    BattlePanelRect skull;
    BattlePanelRect health_track;
    BattlePanelRect health_fill;
};

struct BattlePanelLayout {
    BattlePanelRect viewport;
    int content_height{};
    int scroll_range{};
    int scroll_offset{};
    std::vector<BattlePanelRowLayout> rows;
};

struct BattlePanelScrollbarLayout {
    bool thumb_visible{};
    BattlePanelRect up_arrow;
    BattlePanelRect track;
    BattlePanelRect thumb;
    BattlePanelRect down_arrow;
};

[[nodiscard]] BattlePanelLayout layout_battle_panel_rows(
    std::span<const BattleCreatureRow> rows,
    BattlePanelRect viewport, int scroll_offset);

[[nodiscard]] BattlePanelScrollbarLayout layout_battle_panel_scrollbar(
    BattlePanelRect bounds, int content_height, int viewport_height,
    int scroll_offset);

}
