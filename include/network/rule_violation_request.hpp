#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace tibia::network {

[[nodiscard]] std::vector<std::uint8_t>
encode_state6_rule_violation_request(std::string_view player_name,
    std::uint8_t reason_id, std::uint8_t action_id, std::string_view comment,
    std::uint32_t statement_id, std::uint8_t ip_banishment);

}
