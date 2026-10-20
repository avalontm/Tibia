#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tibia::client {

inline constexpr std::size_t hotkey_count = 36;

struct HotkeyPreference {
    std::string text;
    bool send_automatically{};
    std::array<std::int32_t, 3> data{};
};

struct ParsedHotkeyPreference {
    std::size_t index{};
    HotkeyPreference preference;
};

[[nodiscard]] ParsedHotkeyPreference parse_hotkey_preference(
    std::string_view value, std::size_t line_number);
[[nodiscard]] std::string serialize_hotkey_preference(
    std::size_t index, const HotkeyPreference& preference);
[[nodiscard]] bool has_persisted_hotkey(const HotkeyPreference& preference) noexcept;

}
