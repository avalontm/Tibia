#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace tibia::client {

enum class ConsoleChatInputKind : std::uint8_t {
    empty,
    speech,
    filter_command,
    requires_channel,
    account_data_warning
};

enum class ConsoleFilterOperation : std::uint8_t {
    none,
    invite,
    exclude
};

struct ConsoleChatInput {
    ConsoleChatInputKind kind{ConsoleChatInputKind::empty};
    ConsoleFilterOperation filter_operation{ConsoleFilterOperation::none};
    std::uint8_t mode{1};
    std::string addressee;
    std::string text;
};

[[nodiscard]] ConsoleChatInput parse_default_console_input(
    std::string_view input, std::string_view account_number = {},
    std::string_view password = {}, std::uint8_t default_speech_mode = 1);

}
