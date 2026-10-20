#pragma once

#include "client/message_line_list.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tibia::client {

struct CommunicationBufferRoute {
    MessageLineList* lines{};
    std::uint8_t mode_filter{};
    std::int32_t identifier_filter{-1};
    std::string addressee_filter;
    bool active{};
    bool server_messages_enabled{};
    std::optional<std::uint16_t> channel_identifier;
};

struct CommunicationRoutingSettings {
    bool mode_13_and_14_enabled{true};
    bool mode_15_enabled{};
    bool mode_16_enabled{};
};

struct CommunicationRouteResult {
    bool sent_to_primary{};
    std::vector<std::size_t> matching_buffers;
};

enum class TalkboxFilterGroup : std::uint8_t {
    display_override,
    secondary_display_override,
    suppress_bubble
};

struct TalkboxSpeakerFilter {
    std::string name;
    bool active{};
};

class TalkboxFilterRegistry {
public:
    void set_speaker_filter(TalkboxFilterGroup group, std::string_view speaker_name,
        bool active = true);
    [[nodiscard]] std::vector<TalkboxSpeakerFilter> speaker_filters(
        TalkboxFilterGroup group) const;
    [[nodiscard]] bool remove_speaker_filter(TalkboxFilterGroup group,
        std::size_t index);
    void clear(TalkboxFilterGroup group) noexcept;
    [[nodiscard]] bool should_display_bubble(std::uint8_t mode,
        std::string_view speaker_name, std::int32_t identifier = -1) const noexcept;

    bool display_override_enabled{};
    bool secondary_display_override_enabled{};
    bool suppression_enabled{};

private:
    [[nodiscard]] bool contains(const std::vector<CommunicationBufferRoute>& filters,
        std::uint8_t mode, std::string_view speaker_name,
        std::int32_t identifier) const noexcept;

    std::vector<CommunicationBufferRoute> display_override_filters_;
    std::vector<CommunicationBufferRoute> secondary_display_override_filters_;
    std::vector<CommunicationBufferRoute> suppression_filters_;
};

[[nodiscard]] bool matches_communication_buffer(const CommunicationBufferRoute& buffer,
    std::uint8_t mode, std::string_view addressee, std::int32_t identifier) noexcept;
[[nodiscard]] CommunicationRouteResult route_communication_message(
    MessageLineList* primary, std::span<CommunicationBufferRoute> buffers,
    MessageLine message, std::int32_t identifier,
    const CommunicationRoutingSettings& settings,
    bool primary_server_messages_enabled = true);

}
