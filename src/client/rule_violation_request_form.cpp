#include "client/rule_violation_request_form.hpp"

#include <algorithm>

namespace tibia::client {
namespace {

constexpr std::array<std::wstring_view, 32> reason_labels{
    L"1a) Offensive name",
    L"1b) Name containing part of sentence",
    L"1b) Name with nonsensical letter combination",
    L"1b) Invalid name format",
    L"1c) Name not describing person",
    L"1c) Name of celebrity",
    L"1c) Name referring to country",
    L"1c) Name to fake player identity",
    L"1c) Name to fake official position",
    L"2a) Offensive statement",
    L"2b) Spamming",
    L"2c) Advertisement not related to game",
    L"2c) Real money advertisement",
    L"2d) Non-English public statement",
    L"2d) Off-topic public statement",
    L"2e) Inciting rule violation",
    L"3a) Bug abuse",
    L"3b) Game weakness abuse",
    L"3c) Macro use",
    L"3d) Using unofficial software to play",
    L"3e) Hacking",
    L"3f) Multi-clienting",
    L"3g) Account trading",
    L"3g) Account sharing",
    L"4a) Threatening gamemaster",
    L"4b) Pretending to have official position",
    L"4b) Pretending to have influence on gamemaster",
    L"4c) False report to gamemaster",
    L"Excessive unjustified player killing",
    L"Destructive behaviour",
    L"Spoiling auction",
    L"Invalid payment"
};

constexpr std::array<std::wstring_view, 8> action_labels{
    L"Notation",
    L"Name Report",
    L"Banishment",
    L"Name Report + Banishment",
    L"Banishment + Final Warning",
    L"Name Report + Banishment + Final Warning",
    L"Statement Report",
    L"IP Banishment"
};

}

void RuleViolationRequestForm::show(
    const std::array<std::uint8_t, 32>& rights) {
    open = true;
    player_name.clear();
    comment.clear();
    statement.clear();
    server_rights = rights;
    available_reasons.clear();
    available_actions.clear();
    selected_reason = -1;
    selected_action = -1;
    statement_id = 0;
    ip_banishment = false;
    ip_banishment_enabled = false;
    statement_enabled = false;
    statement_reportable = false;
    for (std::uint8_t reason = 0; reason < server_rights.size(); ++reason) {
        if (server_rights[reason] != 0)
            available_reasons.push_back(reason);
    }
}

void RuleViolationRequestForm::show_for_message(
    const std::array<std::uint8_t, 32>& rights, std::wstring_view player,
    std::wstring_view message, std::uint32_t report_statement_id) {
    show(rights);
    player_name.assign(player);
    statement.assign(message);
    statement_id = report_statement_id;
    statement_reportable = report_statement_id != 0;
}

void RuleViolationRequestForm::close() noexcept {
    open = false;
    available_reasons.clear();
    available_actions.clear();
    selected_reason = -1;
    selected_action = -1;
    ip_banishment = false;
    ip_banishment_enabled = false;
    statement_enabled = false;
    statement_reportable = false;
    statement_id = 0;
}

bool RuleViolationRequestForm::select_reason(int reason_id) {
    const auto reason = std::find(available_reasons.begin(),
        available_reasons.end(), reason_id);
    if (reason == available_reasons.end()) return false;
    selected_reason = reason_id;
    selected_action = -1;
    available_actions.clear();
    const auto rights = server_rights[static_cast<std::size_t>(reason_id)];
    for (std::uint8_t action = 0; action < 7; ++action) {
        if ((rights & (1U << action)) != 0)
            available_actions.push_back(action);
    }
    ip_banishment_enabled = (rights & (1U << 7)) != 0;
    statement_enabled = (rights & ((1U << 0) | (1U << 2) | (1U << 6))) != 0 &&
        ((reason_id >= 9 && reason_id <= 27) || reason_id == 29);
    return true;
}

bool RuleViolationRequestForm::select_action(int action_id) noexcept {
    if (std::find(available_actions.begin(), available_actions.end(), action_id) ==
        available_actions.end())
        return false;
    selected_action = action_id;
    return true;
}

void RuleViolationRequestForm::set_ip_banishment(bool enabled) noexcept {
    ip_banishment = ip_banishment_enabled && enabled;
}

RuleViolationValidationError RuleViolationRequestForm::validate(
    bool statement_reportable) const noexcept {
    if (player_name.empty())
        return RuleViolationValidationError::missing_player_name;
    if (selected_reason < 0)
        return RuleViolationValidationError::missing_reason;
    if (selected_action < 0)
        return RuleViolationValidationError::missing_action;
    if (comment.empty())
        return RuleViolationValidationError::missing_comment;
    if (selected_action == 6) {
        if (statement.empty())
            return RuleViolationValidationError::missing_statement;
        if (!statement_reportable)
            return RuleViolationValidationError::statement_not_reportable;
    }
    return RuleViolationValidationError::none;
}

std::wstring_view rule_violation_reason_label(std::uint8_t reason_id) noexcept {
    return reason_id < reason_labels.size() ? reason_labels[reason_id]
                                           : std::wstring_view{};
}

std::wstring_view rule_violation_action_label(std::uint8_t action_id) noexcept {
    return action_id < action_labels.size() ? action_labels[action_id]
                                           : std::wstring_view{};
}

std::wstring_view rule_violation_validation_message(
    RuleViolationValidationError error) noexcept {
    switch (error) {
    case RuleViolationValidationError::missing_player_name:
        return L"No playername given.";
    case RuleViolationValidationError::missing_reason:
        return L"No reason has been selected.";
    case RuleViolationValidationError::missing_action:
        return L"No action has been selected.";
    case RuleViolationValidationError::missing_comment:
        return L"You must enter a comment.";
    case RuleViolationValidationError::missing_statement:
        return L"No statement has been selected.";
    case RuleViolationValidationError::statement_not_reportable:
        return L"You cannot report this statement.";
    case RuleViolationValidationError::none:
        return {};
    }
    return {};
}

}
