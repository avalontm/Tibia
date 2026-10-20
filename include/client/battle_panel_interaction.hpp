#pragma once

#include "client/battle_panel_layout.hpp"
#include "client/content_window_stack_layout.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace tibia::client {

enum class BattleTargetAction : std::uint16_t {
    attack = 0x2714,
    stop_attack = 0x2715,
    follow = 0x2716,
    stop_follow = 0x2717,
    message_to = 0x2774,
    add_vip = 0x271f,
    toggle_ignore = 0x2775,
    copy_name = 0x27db
};

struct BattleTargetState {
    std::uint32_t attack_target{};
    std::uint32_t follow_target{};
    std::uint32_t hovered_creature{};
};

struct BattleContextMenuEntry {
    BattleTargetAction action{};
    std::wstring label;
    ContentWindowRect rect;
    bool separator{};
};

struct BattleContextMenuOption {
    BattleTargetAction action{};
    std::wstring label;
};

struct BattleContextMenuPlan {
    ContentWindowRect window;
    std::uint32_t creature_id{};
    std::vector<BattleContextMenuEntry> entries;
};

inline constexpr int battle_context_menu_border = 4;
inline constexpr int battle_context_menu_text_padding = 2;
inline constexpr int battle_context_menu_action_width_padding = 44;
inline constexpr int battle_context_menu_separator_width = 50;
inline constexpr int battle_context_menu_separator_height = 8;

[[nodiscard]] std::uint32_t battle_panel_creature_at(
    const BattlePanelLayout& layout,
    std::span<const BattleCreatureRow> rows,
    int x,
    int y) noexcept;

[[nodiscard]] BattleContextMenuPlan make_battle_context_menu_plan(
    const BattleTargetState& targets,
    std::uint32_t creature_id,
    std::vector<BattleContextMenuOption> player_options,
    std::span<const int> text_widths,
    int font_height,
    int anchor_x,
    int anchor_y,
    int client_width,
    int client_height);

[[nodiscard]] bool apply_battle_target_action(
    BattleTargetState& targets,
    BattleTargetAction action,
    std::uint32_t creature_id) noexcept;

[[nodiscard]] std::array<std::uint8_t, 5> battle_target_payload(
    BattleTargetAction action,
    std::uint32_t creature_id) noexcept;

}
