#include "client/content_window_state.hpp"

#include "client/content_window_frame_layout.hpp"

#include <algorithm>

namespace tibia::client {

std::optional<ContentWindowType> content_window_type_from_id(
    std::int32_t type) noexcept {
    switch (type) {
    case 6: return ContentWindowType::skills;
    case 7: return ContentWindowType::battle;
    case 8: return ContentWindowType::trade;
    case 9: return ContentWindowType::vip;
    default: return std::nullopt;
    }
}

std::string_view content_window_title(ContentWindowType type) noexcept {
    switch (type) {
    case ContentWindowType::skills: return "Skills";
    case ContentWindowType::battle: return "Battle";
    case ContentWindowType::trade: return "Trade";
    case ContentWindowType::vip: return "VIP";
    }
    return {};
}

std::int32_t content_window_saved_height(
    std::span<const ContentWindowPreference> preferences,
    ContentWindowType type) noexcept {
    const auto id = static_cast<std::int32_t>(type);
    const auto found = std::find_if(preferences.begin(), preferences.end(),
        [id](const auto& preference) { return preference.type == id; });
    return found == preferences.end() ? 0 : found->value;
}

void save_content_window_height(std::vector<ContentWindowPreference>& preferences,
    ContentWindowType type, std::int32_t height) {
    const auto id = static_cast<std::int32_t>(type);
    const auto body_height = std::max(0, height -
        content_window_frame_header_height);
    const auto found = std::find_if(preferences.begin(), preferences.end(),
        [id](const auto& preference) { return preference.type == id; });
    if (found != preferences.end()) {
        found->value = body_height;
        return;
    }
    preferences.push_back({id, body_height});
}

std::int32_t resolve_content_window_height(std::int32_t saved_height,
    std::int32_t natural_height, std::int32_t minimum_height) noexcept {
    const auto height = saved_height == 0 ? natural_height : saved_height;
    return std::max(height, minimum_height);
}

std::vector<RestoredContentWindow> restore_content_windows(
    std::span<const ContentWindowPreference> preferences) {
    std::vector<RestoredContentWindow> result;
    result.reserve(preferences.size());
    for (auto index = preferences.size(); index > 0; --index) {
        const auto id = preferences[index - 1].type;
        if (std::find_if(restorable_content_window_types.begin(),
                restorable_content_window_types.end(), [id](const auto type) {
                    return id == static_cast<std::int32_t>(type);
                }) == restorable_content_window_types.end())
            continue;
        const auto type = *content_window_type_from_id(id);
        if (std::find_if(result.begin(), result.end(), [type](const auto& window) {
                return window.type == type;
            }) != result.end())
            continue;
        const auto body_height = content_window_saved_height(preferences, type);
        result.push_back({type, body_height > 0
            ? body_height + content_window_frame_header_height : 0});
    }
    return result;
}

}
