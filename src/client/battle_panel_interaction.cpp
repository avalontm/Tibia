#include "client/battle_panel_interaction.hpp"

#include <algorithm>
#include <cstddef>

namespace tibia::client {

std::vector<BattleContextMenuOption> battle_party_context_options(
    std::uint8_t local_party_flag,
    std::uint8_t target_party_flag,
    std::wstring_view player_name) {
    switch (local_party_flag) {
    case 0:
    case 2:
        if (target_party_flag == 1)
            return {{BattleTargetAction::join_party,
                L"Join " + std::wstring(player_name) + L"'s Party"}};
        return {{BattleTargetAction::invite_party, L"Invite to Party"}};
    case 1:
        if (target_party_flag == 2)
            return {{BattleTargetAction::revoke_party_invitation,
                L"Revoke " + std::wstring(player_name) + L"'s Invitation"}};
        return {};
    case 3:
        return {};
    case 4:
        if (target_party_flag == 2)
            return {{BattleTargetAction::revoke_party_invitation,
                L"Revoke " + std::wstring(player_name) + L"'s Invitation"}};
        if (target_party_flag == 3)
            return {{BattleTargetAction::pass_party_leadership,
                L"Pass Leadership to " + std::wstring(player_name)}};
        return {{BattleTargetAction::invite_party, L"Invite to Party"}};
    default:
        return {};
    }
}

std::uint32_t battle_panel_creature_at(
    const BattlePanelLayout& layout,
    std::span<const BattleCreatureRow> rows,
    int x,
    int y) noexcept {
    for (const auto& row : layout.rows) {
        if (row.index >= rows.size()) continue;
        if (x < row.clip.x || x >= row.clip.x + row.clip.width ||
            y < row.clip.y || y >= row.clip.y + row.clip.height)
            continue;
        return rows[row.index].creature.creature_id;
    }
    return 0;
}

BattleContextMenuPlan make_battle_context_menu_plan(
    const BattleTargetState& targets,
    std::uint32_t creature_id,
    std::vector<BattleContextMenuOption> player_options,
    std::span<const int> text_widths,
    int font_height,
    int anchor_x,
    int anchor_y,
    int client_width,
    int client_height,
    bool include_rule_violation) {
    const auto attack_action = targets.attack_target == creature_id
        ? BattleTargetAction::stop_attack : BattleTargetAction::attack;
    const auto follow_action = targets.follow_target == creature_id
        ? BattleTargetAction::stop_follow : BattleTargetAction::follow;
    const std::array<std::wstring, 2> labels{
        attack_action == BattleTargetAction::attack ? L"Attack" : L"Stop Attack",
        follow_action == BattleTargetAction::follow ? L"Follow" : L"Stop Follow"
    };
    const auto row_height = std::max(0, font_height) +
        battle_context_menu_text_padding * 2;
    auto width = battle_context_menu_separator_width;
    for (const auto text_width : text_widths)
        width = std::max(width, std::max(0, text_width) +
            battle_context_menu_action_width_padding);
    width += battle_context_menu_border * 2;
    const auto has_player_options = !player_options.empty();
    const auto item_count = 3U + static_cast<unsigned>(player_options.size()) +
        static_cast<unsigned>(include_rule_violation);
    const auto separator_count = 1U + static_cast<unsigned>(has_player_options);
    const auto height = battle_context_menu_border * 2 +
        row_height * static_cast<int>(item_count) +
        battle_context_menu_separator_height * static_cast<int>(separator_count);
    auto x = anchor_x;
    auto y = anchor_y;
    if (x + width > client_width) x = client_width - width;
    if (y + height > client_height) y = client_height - height;
    x = std::max(0, x);
    y = std::max(0, y);

    BattleContextMenuPlan plan;
    plan.window = {x, y, width, height};
    plan.creature_id = creature_id;
    plan.entries.reserve(item_count + separator_count);
    auto row_y = y + battle_context_menu_border;
    plan.entries.push_back({attack_action, labels[0],
        {x + battle_context_menu_border, row_y,
            width - battle_context_menu_border * 2, row_height}, false});
    row_y += row_height;
    plan.entries.push_back({follow_action, labels[1],
        {x + battle_context_menu_border, row_y,
            width - battle_context_menu_border * 2, row_height}, false});
    row_y += row_height;
    plan.entries.push_back({{}, {},
        {x + battle_context_menu_border, row_y,
            width - battle_context_menu_border * 2,
            battle_context_menu_separator_height}, true});
    row_y += battle_context_menu_separator_height;
    if (has_player_options) {
        for (auto& option : player_options) {
            plan.entries.push_back({option.action, std::move(option.label),
                {x + battle_context_menu_border, row_y,
                    width - battle_context_menu_border * 2, row_height}, false});
            row_y += row_height;
        }
        plan.entries.push_back({{}, {},
            {x + battle_context_menu_border, row_y,
                width - battle_context_menu_border * 2,
                battle_context_menu_separator_height}, true});
        row_y += battle_context_menu_separator_height;
    } else {
        plan.entries.push_back({{}, {},
            {x + battle_context_menu_border, row_y,
                width - battle_context_menu_border * 2,
                battle_context_menu_separator_height}, true});
        row_y += battle_context_menu_separator_height;
    }
    plan.entries.push_back({BattleTargetAction::copy_name, L"Copy Name",
        {x + battle_context_menu_border, row_y,
            width - battle_context_menu_border * 2, row_height}, false});
    if (include_rule_violation) {
        row_y += row_height;
        plan.entries.push_back({BattleTargetAction::rule_violation,
            L"Rule Violation",
            {x + battle_context_menu_border, row_y,
                width - battle_context_menu_border * 2, row_height}, false});
    }
    return plan;
}

bool apply_battle_target_action(
    BattleTargetState& targets,
    BattleTargetAction action,
    std::uint32_t creature_id) noexcept {
    switch (action) {
    case BattleTargetAction::attack:
        targets.attack_target = creature_id;
        targets.follow_target = 0;
        return true;
    case BattleTargetAction::stop_attack:
        targets.attack_target = 0;
        targets.follow_target = 0;
        return true;
    case BattleTargetAction::follow:
        targets.attack_target = 0;
        targets.follow_target = creature_id;
        return true;
    case BattleTargetAction::stop_follow:
        targets.attack_target = 0;
        targets.follow_target = 0;
        return true;
    case BattleTargetAction::copy_name:
    case BattleTargetAction::message_to:
    case BattleTargetAction::add_vip:
    case BattleTargetAction::toggle_ignore:
    case BattleTargetAction::rule_violation:
        return false;
    }
    return false;
}

std::array<std::uint8_t, 5> battle_target_payload(
    BattleTargetAction action,
    std::uint32_t creature_id) noexcept {
    const auto opcode = action == BattleTargetAction::follow ||
        action == BattleTargetAction::stop_follow ? 0xa2U : 0xa1U;
    if (action == BattleTargetAction::stop_attack ||
        action == BattleTargetAction::stop_follow)
        creature_id = 0;
    return {
        static_cast<std::uint8_t>(opcode),
        static_cast<std::uint8_t>(creature_id),
        static_cast<std::uint8_t>(creature_id >> 8U),
        static_cast<std::uint8_t>(creature_id >> 16U),
        static_cast<std::uint8_t>(creature_id >> 24U)
    };
}

}
