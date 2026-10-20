#include "network/status_line_state.hpp"

#include <cstdlib>

namespace tibia::network {

bool StatusLineState::consume(std::string_view message) {
    const std::lock_guard lock(mutex_);
    const auto terminator = message.find('\0');
    message = message.substr(0, terminator);
    const auto line_feed = message.find('\n');
    if (line_feed == std::string_view::npos) return false;

    const std::string number_text(message.substr(0, line_feed));
    const auto parsed_value = static_cast<std::int32_t>(std::strtol(number_text.c_str(), nullptr, 10));
    const bool changed = parsed_value != value_;
    value_ = parsed_value;
    text_.assign(message.substr(line_feed + 1));
    has_text_ = true;
    return changed;
}

std::int32_t StatusLineState::value() const {
    const std::lock_guard lock(mutex_);
    return value_;
}

StatusLineSnapshot StatusLineState::snapshot() const {
    const std::lock_guard lock(mutex_);
    return {value_, has_text_, text_};
}

}
