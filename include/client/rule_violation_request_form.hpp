#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tibia::client {

enum class RuleViolationValidationError : std::uint8_t {
    none,
    missing_player_name,
    missing_reason,
    missing_action,
    missing_comment,
    missing_statement,
    statement_not_reportable
};

struct RuleViolationRequestForm {
    bool open{};
    std::wstring player_name;
    std::wstring comment;
    std::wstring statement;
    std::array<std::uint8_t, 32> server_rights{};
    std::vector<std::uint8_t> available_reasons;
    std::vector<std::uint8_t> available_actions;
    int selected_reason{-1};
    int selected_action{-1};
    std::uint32_t statement_id{};
    bool ip_banishment{};
    bool ip_banishment_enabled{};
    bool statement_enabled{};
    bool statement_reportable{};

    void show(const std::array<std::uint8_t, 32>& rights);
    void show_for_message(const std::array<std::uint8_t, 32>& rights,
        std::wstring_view player, std::wstring_view message,
        std::uint32_t report_statement_id);
    void close() noexcept;
    [[nodiscard]] bool select_reason(int reason_id);
    [[nodiscard]] bool select_action(int action_id) noexcept;
    void set_ip_banishment(bool enabled) noexcept;
    [[nodiscard]] RuleViolationValidationError validate(
        bool statement_reportable) const noexcept;
};

[[nodiscard]] std::wstring_view rule_violation_reason_label(
    std::uint8_t reason_id) noexcept;
[[nodiscard]] std::wstring_view rule_violation_action_label(
    std::uint8_t action_id) noexcept;
[[nodiscard]] std::wstring_view rule_violation_validation_message(
    RuleViolationValidationError error) noexcept;

}
