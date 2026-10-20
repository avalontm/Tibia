#include "client/communication_router.hpp"

#include <algorithm>

namespace tibia::client {
namespace {

std::uint8_t fold_case(std::uint8_t value) noexcept {
    if ((value >= 0x41 && value <= 0x5A) || (value >= 0xC1 && value <= 0xDE))
        return static_cast<std::uint8_t>(value + 0x20);
    return value;
}

bool equal_case_insensitive(std::string_view left, std::string_view right) noexcept {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (fold_case(static_cast<std::uint8_t>(left[i])) !=
            fold_case(static_cast<std::uint8_t>(right[i]))) return false;
    }
    return true;
}

std::uint8_t buffer_filter_mode(std::uint8_t mode) noexcept {
    if (mode == 10 || mode == 12 || mode == 14 || mode == 0x0C) return 5;
    return mode;
}

}

bool matches_communication_buffer(const CommunicationBufferRoute& buffer,
    std::uint8_t mode, std::string_view addressee, std::int32_t identifier) noexcept {
    if (mode == 9 || mode == 0x0A || mode == 0x0B || mode == 0x0C) return false;
    if (buffer.mode_filter != 0 && buffer.mode_filter != mode) return false;
    if (buffer.identifier_filter != -1 && buffer.identifier_filter != identifier) return false;
    if (buffer.addressee_filter.empty()) return true;
    if (addressee.empty()) return false;
    return equal_case_insensitive(buffer.addressee_filter, addressee);
}

void TalkboxFilterRegistry::set_speaker_filter(TalkboxFilterGroup group,
    std::string_view speaker_name, bool active) {
    auto* collection = &suppression_filters_;
    if (group == TalkboxFilterGroup::display_override)
        collection = &display_override_filters_;
    else if (group == TalkboxFilterGroup::secondary_display_override)
        collection = &secondary_display_override_filters_;

    const auto name = speaker_name.substr(0, 30);
    const auto existing = std::find_if(collection->begin(), collection->end(),
        [name](const auto& filter) {
            return filter.mode_filter == 0 && filter.identifier_filter == -1 &&
                equal_case_insensitive(filter.addressee_filter, name);
        });
    if (existing != collection->end()) {
        existing->active = active;
        return;
    }

    CommunicationBufferRoute filter;
    filter.mode_filter = 0;
    filter.identifier_filter = -1;
    filter.addressee_filter.assign(name);
    filter.active = active;
    collection->push_back(std::move(filter));
}

std::vector<TalkboxSpeakerFilter> TalkboxFilterRegistry::speaker_filters(
    TalkboxFilterGroup group) const {
    const auto* collection = &suppression_filters_;
    if (group == TalkboxFilterGroup::display_override)
        collection = &display_override_filters_;
    else if (group == TalkboxFilterGroup::secondary_display_override)
        collection = &secondary_display_override_filters_;

    std::vector<TalkboxSpeakerFilter> result;
    result.reserve(collection->size());
    for (const auto& filter : *collection)
        result.push_back({filter.addressee_filter, filter.active});
    return result;
}

bool TalkboxFilterRegistry::remove_speaker_filter(TalkboxFilterGroup group,
    std::size_t index) {
    auto* collection = &suppression_filters_;
    if (group == TalkboxFilterGroup::display_override)
        collection = &display_override_filters_;
    else if (group == TalkboxFilterGroup::secondary_display_override)
        collection = &secondary_display_override_filters_;
    if (index >= collection->size()) return false;
    collection->erase(collection->begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void TalkboxFilterRegistry::clear(TalkboxFilterGroup group) noexcept {
    if (group == TalkboxFilterGroup::display_override)
        display_override_filters_.clear();
    else if (group == TalkboxFilterGroup::secondary_display_override)
        secondary_display_override_filters_.clear();
    else
        suppression_filters_.clear();
}

bool TalkboxFilterRegistry::should_display_bubble(std::uint8_t mode,
    std::string_view speaker_name, std::int32_t identifier) const noexcept {
    if (display_override_enabled) {
        if (secondary_display_override_enabled && contains(
            secondary_display_override_filters_, mode, speaker_name, identifier)) return true;
        if (contains(display_override_filters_, mode, speaker_name, identifier)) return true;
    }
    if (suppression_enabled && contains(
        suppression_filters_, mode, speaker_name, identifier)) return false;
    return true;
}

bool TalkboxFilterRegistry::contains(
    const std::vector<CommunicationBufferRoute>& filters, std::uint8_t mode,
    std::string_view speaker_name, std::int32_t identifier) const noexcept {
    return std::any_of(filters.begin(), filters.end(), [&](const auto& filter) {
        return filter.active && matches_communication_buffer(
            filter, mode, speaker_name, identifier);
    });
}

CommunicationRouteResult route_communication_message(MessageLineList* primary,
    std::span<CommunicationBufferRoute> buffers, MessageLine message,
    std::int32_t identifier, const CommunicationRoutingSettings& settings,
    bool primary_server_messages_enabled) {
    const auto mode = message.mode;
    const bool server_message = mode >= 0x12 && mode <= 0x16;
    bool send_to_primary = true;
    bool inspect_buffers = true;
    switch (mode) {
    case 0x12:
        break;
    case 0x13:
    case 0x14:
        send_to_primary = settings.mode_13_and_14_enabled;
        inspect_buffers = send_to_primary;
        break;
    case 0x15:
        send_to_primary = settings.mode_15_enabled;
        inspect_buffers = send_to_primary;
        break;
    case 0x16:
        send_to_primary = settings.mode_16_enabled;
        inspect_buffers = send_to_primary;
        break;
    case 0x17:
        send_to_primary = false;
        inspect_buffers = false;
        break;
    case 5: case 6: case 7: case 8: case 10: case 0x0C: case 0x0E:
        send_to_primary = false;
        break;
    default:
        break;
    }

    CommunicationRouteResult result;
    if (send_to_primary && (!server_message || primary_server_messages_enabled) &&
        primary != nullptr) {
        result.sent_to_primary = primary->append(message);
    }
    if (!inspect_buffers) return result;

    const auto effective_mode = buffer_filter_mode(mode);
    auto addressee = std::string_view(message.speaker_name);
    if (mode == 0x0C || mode == 0x0E) addressee = {};
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        auto& buffer = buffers[i];
        if (!buffer.active || buffer.lines == nullptr) continue;
        if (server_message && !buffer.server_messages_enabled) continue;
        if (!matches_communication_buffer(buffer, effective_mode, addressee, identifier)) continue;
        const auto accepted = buffer.lines->append(message);
        if (accepted) result.matching_buffers.push_back(i);
    }
    return result;
}

}
