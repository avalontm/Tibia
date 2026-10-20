#pragma once

#include "client/communication_display_preferences.hpp"
#include "client/communication_router.hpp"
#include "network/message_dispatcher.hpp"

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tibia::client {

class CommunicationSession {
public:
    CommunicationSession();

    [[nodiscard]] CommunicationRouteResult apply_speech(
        const network::State6SpeechMessage& message, std::time_t current_time);
    [[nodiscard]] CommunicationRouteResult apply_local_message(
        std::uint8_t mode, std::string_view text, std::time_t current_time);
    [[nodiscard]] CommunicationRouteResult apply_control(
        const network::State6CommunicationControl& update, std::time_t current_time);
    void replace_available_channels(const std::vector<network::State6ChannelEntry>& channels);
    void open_channel(const network::State6ChannelOpen& channel);
    [[nodiscard]] bool select_channel(std::uint16_t identifier) noexcept;
    void select_default_channel() noexcept;
    [[nodiscard]] bool toggle_server_messages(
        std::optional<std::uint16_t> identifier) noexcept;
    [[nodiscard]] bool server_messages_enabled(
        std::optional<std::uint16_t> identifier) const noexcept;
    [[nodiscard]] CommunicationRouteResult close_channel(
        std::uint16_t identifier, std::time_t current_time);

    [[nodiscard]] MessageLineList& primary_messages() noexcept;
    [[nodiscard]] const MessageLineList& primary_messages() const noexcept;
    [[nodiscard]] const std::vector<network::State6ChannelEntry>& available_channels() const noexcept;
    [[nodiscard]] const std::vector<network::State6ChannelOpen>& open_channels() const noexcept;
    [[nodiscard]] const MessageLineList& channel_messages() const noexcept;
    [[nodiscard]] const MessageLineList* channel_messages(std::uint16_t identifier) const noexcept;
    [[nodiscard]] std::optional<std::uint16_t> channel_buffer_identifier() const noexcept;
    [[nodiscard]] bool has_active_private_channel_filter() const noexcept;
    [[nodiscard]] TalkboxFilterRegistry& talkbox_filters() noexcept;
    [[nodiscard]] CommunicationDisplayPreferences& display_preferences() noexcept;
    [[nodiscard]] std::string format_console_message(const MessageLine& message) const;
    [[nodiscard]] bool should_display_talkbox(
        const network::State6SpeechMessage& message) const noexcept;
    void replace_talkbox_filters(std::span<const std::string> white_list,
        std::span<const std::string> ignore_list);
    void replace_talkbox_filters(std::span<const TalkboxSpeakerFilter> white_list,
        std::span<const TalkboxSpeakerFilter> ignore_list);
    [[nodiscard]] std::vector<CommunicationBufferRoute>& buffers() noexcept;
    [[nodiscard]] CommunicationRoutingSettings& settings() noexcept;

private:
    [[nodiscard]] CommunicationRouteResult route(MessageLine message,
        std::int32_t identifier);

    MessageLineList primary_messages_;
    MessageLineList empty_channel_messages_{0};
    std::vector<network::State6ChannelEntry> available_channels_;
    std::vector<network::State6ChannelOpen> open_channels_;
    std::map<std::uint16_t, MessageLineList> channel_message_buffers_;
    std::vector<CommunicationBufferRoute> buffers_;
    CommunicationRoutingSettings settings_;
    CommunicationDisplayPreferences display_preferences_;
    TalkboxFilterRegistry talkbox_filters_;
    std::optional<std::uint16_t> channel_buffer_identifier_;
    bool primary_server_messages_enabled_{};
    std::vector<std::uint16_t> active_filter_channel_identifiers_;
};

struct CommunicationLocalMessage {
    std::uint8_t mode{};
    std::string text;
};

struct CommunicationStatusMessage {
    std::string text;
    bool persistent{};
};

struct CommunicationChannelView {
    std::vector<network::State6ChannelOpen> channels;
    std::vector<std::size_t> channel_message_counts;
    std::optional<std::uint16_t> selected_identifier;
    std::vector<MessageLine> messages;
    bool server_messages_enabled{};
};

class CommunicationSessionMailbox {
public:
    void publish_ignore_list(std::vector<std::string> names);
    void publish_private_channel_filter_state(bool available);
    [[nodiscard]] bool private_channel_filter_available() const;
    void publish_active_channel(std::optional<std::uint16_t> identifier);
    [[nodiscard]] std::optional<std::uint16_t> active_channel() const;
    void publish_channel_view(CommunicationChannelView view);
    [[nodiscard]] CommunicationChannelView channel_view() const;
    void publish_available_channels(
        std::vector<network::State6ChannelEntry> channels);
    [[nodiscard]] std::vector<network::State6ChannelEntry> available_channels() const;
    void request_channel_selection(std::optional<std::uint16_t> identifier);
    [[nodiscard]] std::optional<std::optional<std::uint16_t>> take_channel_selection();
    void request_channel_close(std::uint16_t identifier);
    [[nodiscard]] std::optional<std::uint16_t> take_channel_close();
    void request_server_messages_toggle(std::optional<std::uint16_t> identifier);
    [[nodiscard]] std::optional<std::optional<std::uint16_t>>
    take_server_messages_toggle();
    [[nodiscard]] bool read_ignore_list_if_changed(std::uint64_t& revision,
        std::vector<std::string>& names) const;
    void enqueue_local_message(std::uint8_t mode, std::string text);
    [[nodiscard]] std::vector<CommunicationLocalMessage> take_local_messages();
    void enqueue_status_message(std::string text, bool persistent = false);
    [[nodiscard]] std::vector<CommunicationStatusMessage> take_status_messages();

private:
    mutable std::mutex mutex_;
    std::vector<std::string> ignore_names_;
    std::vector<CommunicationLocalMessage> local_messages_;
    std::vector<CommunicationStatusMessage> status_messages_;
    bool private_channel_filter_available_{};
    std::optional<std::uint16_t> active_channel_;
    CommunicationChannelView channel_view_;
    std::vector<network::State6ChannelEntry> available_channels_;
    std::optional<std::optional<std::uint16_t>> requested_channel_selection_;
    std::optional<std::uint16_t> requested_channel_close_;
    std::optional<std::optional<std::uint16_t>> requested_server_messages_toggle_;
    std::uint64_t revision_{};
};

}
