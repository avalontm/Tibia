#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace tibia::client {

struct AvailableHint {
    std::int32_t id{};
    std::wstring_view title;
    std::wstring_view content;
};

[[nodiscard]] const AvailableHint* find_hint(std::int32_t id) noexcept;

[[nodiscard]] std::vector<AvailableHint> available_hints(
    std::span<const std::int32_t> known_hint_ids);

}
