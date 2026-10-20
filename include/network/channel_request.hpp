#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace tibia::network {

[[nodiscard]] std::vector<std::uint8_t> encode_state6_channel_list_request();
[[nodiscard]] std::vector<std::uint8_t> encode_state6_join_channel_request(
    std::uint16_t channel_id);
[[nodiscard]] std::vector<std::uint8_t> encode_state6_private_channel_request(
    std::string_view character_name);
[[nodiscard]] std::vector<std::uint8_t> encode_state6_close_channel_request(
    std::uint16_t channel_id);
[[nodiscard]] std::vector<std::uint8_t> encode_state6_close_private_channel_request(
    std::string_view character_name);
[[nodiscard]] std::vector<std::uint8_t> encode_state6_close_rule_violation_request();
[[nodiscard]] std::vector<std::uint8_t> encode_state6_remove_vip_request(
    std::uint32_t character_id);

}
