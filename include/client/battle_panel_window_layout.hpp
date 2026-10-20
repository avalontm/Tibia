#pragma once

#include "client/battle_panel_layout.hpp"
#include "client/content_window_frame_layout.hpp"
#include "client/content_window_state.hpp"

#include <optional>
#include <span>

namespace tibia::client {

inline constexpr int battle_panel_scrollbar_width = 12;

struct BattlePanelWindowPlan {
    ContentWindowRect window;
    ContentWindowFrameLayout frame;
    BattlePanelLayout panel;
    BattlePanelScrollbarLayout scrollbar;
};

[[nodiscard]] std::optional<BattlePanelWindowPlan> make_battle_panel_window_plan(
    int client_width,
    int client_height,
    int origin_y,
    std::span<const RestoredContentWindow> restored_windows,
    std::span<const BattleCreatureRow> rows,
    int scroll_offset);

}
