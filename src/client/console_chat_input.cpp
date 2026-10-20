#include "client/console_chat_input.hpp"

namespace tibia::client {
namespace {

bool is_console_space(char value) noexcept {
    return value == ' ' || value == '\f' || value == '\n' || value == '\r' ||
        value == '\t' || value == '\v';
}

char uppercase_console_byte(unsigned char value) noexcept {
    if ((value > 0x60 && value < 0x7b) || (value > 0xdf && value != 0xff))
        return static_cast<char>(value - 0x20);
    return static_cast<char>(value);
}

}

ConsoleChatInput parse_default_console_input(std::string_view input,
    std::string_view account_number, std::string_view password,
    std::uint8_t default_speech_mode) {
    std::size_t position = 0;
    while (position < input.size() && is_console_space(input[position])) ++position;
    if (position == input.size()) return {};

    ConsoleChatInput result;
    result.mode = default_speech_mode;
    if (input[position] == '#') {
        ++position;
        const auto command = uppercase_console_byte(
            static_cast<unsigned char>(position < input.size() ? input[position] : 0));
        while (position < input.size() && !is_console_space(input[position])) ++position;
        switch (command) {
        case 'A': result.mode = 13; break;
        case 'B': result.mode = 9; break;
        case 'C': result.mode = 10; break;
        case 'D': result.mode = 14; break;
        case 'I':
            result.mode = 0;
            result.kind = ConsoleChatInputKind::filter_command;
            result.filter_operation = ConsoleFilterOperation::invite;
            break;
        case 'X':
            result.mode = 0;
            result.kind = ConsoleChatInputKind::filter_command;
            result.filter_operation = ConsoleFilterOperation::exclude;
            break;
        case 'S': result.mode = 1; break;
        case 'W': result.mode = 2; break;
        case 'Y': result.mode = 3; break;
        default: break;
        }
        while (position < input.size() && is_console_space(input[position])) ++position;
    } else if (input[position] == '*' || input[position] == '@' ||
        input[position] == '$') {
        const char separator = input[position++];
        const auto addressee_start = position;
        while (position < input.size() && input[position] != separator) ++position;
        if (position == input.size() || position == addressee_start) return {};
        result.addressee.assign(input.substr(addressee_start, position - addressee_start));
        result.mode = separator == '*' ? 4 : separator == '@' ? 11 : 15;
        ++position;
        while (position < input.size() && is_console_space(input[position])) ++position;
    }

    const auto body = input.substr(position);
    if ((!account_number.empty() && body.find(account_number) != std::string_view::npos) ||
        (!password.empty() && body.find(password) != std::string_view::npos)) {
        result.kind = ConsoleChatInputKind::account_data_warning;
        result.filter_operation = ConsoleFilterOperation::none;
        result.mode = 0;
        result.addressee.clear();
        result.text.clear();
        return result;
    }

    if (result.mode == 10 || result.mode == 14) {
        result.kind = ConsoleChatInputKind::requires_channel;
        if (position < input.size()) result.text.assign(input.substr(position));
        return result;
    }
    if (position == input.size()) return {};
    result.text.assign(input.substr(position));
    if (result.kind == ConsoleChatInputKind::filter_command) return result;
    result.kind = ConsoleChatInputKind::speech;
    return result;
}

}
