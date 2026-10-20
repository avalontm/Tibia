#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace tibia::network {

[[nodiscard]] std::optional<std::vector<std::uint8_t>>
encode_state6_speech_request(std::uint8_t mode, std::string_view text,
    std::string_view addressee = {}, std::optional<std::uint16_t> channel_id = std::nullopt);

}
