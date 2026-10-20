#pragma once

#include "client/content_window_frame_layout.hpp"
#include "client/skills_panel_scroll.hpp"
#include "client/content_window_state.hpp"
#include "network/message_dispatcher.hpp"

#include <span>
#include <optional>
#include <vector>

namespace tibia::client {

struct VipPanelRowLayout {
    ContentWindowRect bounds;
    ContentWindowRect clip;
    ContentWindowRect icon;
    std::size_t index{};
};

struct VipPanelWindowPlan {
    ContentWindowRect window;
    ContentWindowFrameLayout frame;
    ContentWindowRect viewport;
    std::vector<VipPanelRowLayout> rows;
    SkillsPanelScrollRange scroll;
    SkillsPanelScrollbarGeometry scrollbar;
};

inline constexpr int vip_panel_row_height = 16;
inline constexpr int vip_panel_body_padding = 10;
inline constexpr int vip_panel_icon_offset = 2;
inline constexpr int vip_panel_icon_size = 12;
inline constexpr int vip_panel_text_offset_x = 20;
inline constexpr int vip_panel_text_offset_y = 4;
inline constexpr int vip_panel_text_max_width = 132;
inline constexpr int vip_panel_scrollbar_width = 12;

[[nodiscard]] std::optional<VipPanelWindowPlan> make_vip_panel_window_plan(
    int client_width,
    int client_height,
    int origin_y,
    std::span<const RestoredContentWindow> restored_windows,
    std::span<const network::State6BuddyEntry> contacts,
    int scroll_value,
    std::optional<std::span<const std::size_t>> contact_order = std::nullopt);

}
