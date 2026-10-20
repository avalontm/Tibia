#pragma once

#include <cstdint>
#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace tibia::client {

enum class ContentWindowType : std::int32_t {
    skills = 6,
    battle = 7,
    trade = 8,
    vip = 9
};

inline constexpr std::array<ContentWindowType, 3> restorable_content_window_types{
    ContentWindowType::skills,
    ContentWindowType::battle,
    ContentWindowType::vip
};

struct ContentWindowPreference {
    std::int32_t type{};
    std::int32_t value{};
};

struct RestoredContentWindow {
    ContentWindowType type{};
    std::int32_t saved_height{};
};

[[nodiscard]] std::optional<ContentWindowType> content_window_type_from_id(
    std::int32_t type) noexcept;
[[nodiscard]] std::string_view content_window_title(ContentWindowType type) noexcept;
[[nodiscard]] std::int32_t content_window_saved_height(
    std::span<const ContentWindowPreference> preferences,
    ContentWindowType type) noexcept;
void save_content_window_height(std::vector<ContentWindowPreference>& preferences,
    ContentWindowType type, std::int32_t height);
[[nodiscard]] std::int32_t resolve_content_window_height(
    std::int32_t saved_height,
    std::int32_t natural_height,
    std::int32_t minimum_height) noexcept;
[[nodiscard]] std::vector<RestoredContentWindow> restore_content_windows(
    std::span<const ContentWindowPreference> preferences);

}
