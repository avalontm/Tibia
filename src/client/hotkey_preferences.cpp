#include "client/hotkey_preferences.hpp"

#include <charconv>
#include <stdexcept>
#include <string>

namespace tibia::client {
namespace {

class HotkeyValueParser {
public:
    HotkeyValueParser(std::string_view value, std::size_t line_number)
        : value_(value), line_number_(line_number) {}

    ParsedHotkeyPreference parse() {
        expect('(');
        const auto index = integer();
        if (index < 0 || index >= static_cast<std::int32_t>(hotkey_count))
            throw std::runtime_error("invalid Tibia.cfg Hotkey index at line " +
                std::to_string(line_number_));
        expect(',');
        auto text = quoted_text();
        if (text.size() >= 256)
            throw std::runtime_error("Tibia.cfg Hotkey text exceeds 255 bytes at line " +
                std::to_string(line_number_));
        HotkeyPreference preference;
        if (!text.empty() && text.back() == '\n') {
            preference.send_automatically = true;
            text.pop_back();
        }
        for (auto& character : text)
            if (character == '\n') character = ' ';
        preference.text = std::move(text);
        skip_space();
        if (take(')')) {
            skip_space();
            if (position_ != value_.size()) fail();
            return {static_cast<std::size_t>(index), std::move(preference)};
        }
        expect(',');
        for (std::size_t field = 0; field < preference.data.size(); ++field) {
            preference.data[field] = integer();
            expect(field + 1 == preference.data.size() ? ')' : ',');
        }
        skip_space();
        if (position_ != value_.size()) fail();
        return {static_cast<std::size_t>(index), std::move(preference)};
    }

private:
    void skip_space() {
        while (position_ < value_.size() &&
            (value_[position_] == ' ' || value_[position_] == '\t' ||
                value_[position_] == '\r' || value_[position_] == '\n')) ++position_;
    }

    bool take(char expected) {
        skip_space();
        if (position_ >= value_.size() || value_[position_] != expected) return false;
        ++position_;
        return true;
    }

    void expect(char expected) {
        if (!take(expected)) fail();
    }

    std::int32_t integer() {
        skip_space();
        std::int32_t result{};
        const auto parsed = std::from_chars(value_.data() + position_,
            value_.data() + value_.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr == value_.data() + position_) fail();
        position_ = static_cast<std::size_t>(parsed.ptr - value_.data());
        return result;
    }

    std::string quoted_text() {
        skip_space();
        if (position_ >= value_.size() || value_[position_++] != '"') fail();
        std::string result;
        bool closed = false;
        while (position_ < value_.size()) {
            const char character = value_[position_++];
            if (character == '"') {
                closed = true;
                break;
            }
            if (character != '\\') {
                result.push_back(character);
                continue;
            }
            if (position_ == value_.size()) fail();
            const char escaped = value_[position_++];
            if (escaped == 'n') result.push_back('\n');
            else if (escaped == '"' || escaped == '\\') result.push_back(escaped);
            else fail();
        }
        if (!closed) fail();
        return result;
    }

    [[noreturn]] void fail() const {
        throw std::runtime_error("invalid Tibia.cfg Hotkey record at line " +
            std::to_string(line_number_));
    }

    std::string_view value_;
    std::size_t line_number_{};
    std::size_t position_{};
};

std::string serialize_text(const HotkeyPreference& preference) {
    std::string text = preference.text;
    if (preference.send_automatically) text.push_back('\n');
    std::string value;
    value.reserve(text.size() + 2);
    value.push_back('"');
    for (const char character : text) {
        if (character == '"' || character == '\\') value.push_back('\\');
        if (character == '\n') value += "\\n";
        else value.push_back(character);
    }
    value.push_back('"');
    return value;
}

}

ParsedHotkeyPreference parse_hotkey_preference(
    std::string_view value, std::size_t line_number) {
    return HotkeyValueParser(value, line_number).parse();
}

std::string serialize_hotkey_preference(std::size_t index,
    const HotkeyPreference& preference) {
    if (index >= hotkey_count)
        throw std::out_of_range("Hotkey index is outside the supported range");
    std::string value = "Hotkey = (" + std::to_string(index) + "," +
        serialize_text(preference);
    for (const auto field : preference.data) value += "," + std::to_string(field);
    value.push_back(')');
    return value;
}

bool has_persisted_hotkey(const HotkeyPreference& preference) noexcept {
    return !preference.text.empty() || preference.data[0] != 0;
}

}
