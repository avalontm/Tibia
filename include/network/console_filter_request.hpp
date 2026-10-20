#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace tibia::network {

enum class State6ConsoleFilterOperation : std::uint8_t {
    invite,
    exclude
};

[[nodiscard]] std::optional<std::vector<std::uint8_t>>
encode_state6_console_filter_request(State6ConsoleFilterOperation operation,
    std::string_view name);

}
