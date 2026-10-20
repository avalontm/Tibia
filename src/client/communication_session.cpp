#include "client/communication_session.hpp"

#include <algorithm>
#include <bit>
#include <utility>

namespace tibia::client {
namespace {

bool uses_speaker_filters(std::uint8_t mode) noexcept {
    switch (mode) {
    case 1: case 2: case 3: case 4: case 5: case 9: case 10: case 11:
    case 12: case 14: case 0x10: case 0x11:
        return true;
    default:
        return false;
    }
}

}

CommunicationSession::CommunicationSession() : primary_messages_(1000) {}

CommunicationRouteResult CommunicationSession::apply_local_message(
    std::uint8_t mode, std::string_view text, std::time_t current_time) {
    auto line = make_message_line(0, mode, {}, 0, std::string(text), 0, current_time);
    return route(std::move(line), -1);
}

void CommunicationSessionMailbox::publish_ignore_list(std::vector<std::string> names) {
    const std::lock_guard lock(mutex_);
    ignore_names_ = std::move(names);
    ++revision_;
}

bool CommunicationSessionMailbox::read_ignore_list_if_changed(std::uint64_t& revision,
    std::vector<std::string>& names) const {
    const std::lock_guard lock(mutex_);
    if (revision == revision_) return false;
    names = ignore_names_;
    revision = revision_;
    return true;
}

void CommunicationSessionMailbox::publish_private_channel_filter_state(bool available) {
    const std::lock_guard lock(mutex_);
    private_channel_filter_available_ = available;
}

bool CommunicationSessionMailbox::private_channel_filter_available() const {
    const std::lock_guard lock(mutex_);
    return private_channel_filter_available_;
}

void CommunicationSessionMailbox::publish_active_channel(
    std::optional<std::uint16_t> identifier) {
    const std::lock_guard lock(mutex_);
    active_channel_ = identifier;
}

std::optional<std::uint16_t> CommunicationSessionMailbox::active_channel() const {
    const std::lock_guard lock(mutex_);
    return active_channel_;
}

void CommunicationSessionMailbox::publish_channel_view(CommunicationChannelView view) {
    const std::lock_guard lock(mutex_);
    channel_view_ = std::move(view);
}

CommunicationChannelView CommunicationSessionMailbox::channel_view() const {
    const std::lock_guard lock(mutex_);
    return channel_view_;
}

void CommunicationSessionMailbox::publish_available_channels(
    std::vector<network::State6ChannelEntry> channels) {
    const std::lock_guard lock(mutex_);
    available_channels_ = std::move(channels);
}

std::vector<network::State6ChannelEntry>
CommunicationSessionMailbox::available_channels() const {
    const std::lock_guard lock(mutex_);
    return available_channels_;
}

void CommunicationSessionMailbox::request_channel_selection(
    std::optional<std::uint16_t> identifier) {
    const std::lock_guard lock(mutex_);
    requested_channel_selection_ = identifier;
}

std::optional<std::optional<std::uint16_t>>
CommunicationSessionMailbox::take_channel_selection() {
    const std::lock_guard lock(mutex_);
    auto selection = std::move(requested_channel_selection_);
    requested_channel_selection_.reset();
    return selection;
}

void CommunicationSessionMailbox::request_channel_close(std::uint16_t identifier) {
    const std::lock_guard lock(mutex_);
    requested_channel_close_ = identifier;
}

std::optional<std::uint16_t> CommunicationSessionMailbox::take_channel_close() {
    const std::lock_guard lock(mutex_);
    const auto identifier = requested_channel_close_;
    requested_channel_close_.reset();
    return identifier;
}

void CommunicationSessionMailbox::request_server_messages_toggle(
    std::optional<std::uint16_t> identifier) {
    const std::lock_guard lock(mutex_);
    requested_server_messages_toggle_ = identifier;
}

std::optional<std::optional<std::uint16_t>>
CommunicationSessionMailbox::take_server_messages_toggle() {
    const std::lock_guard lock(mutex_);
    auto request = std::move(requested_server_messages_toggle_);
    requested_server_messages_toggle_.reset();
    return request;
}

void CommunicationSessionMailbox::enqueue_local_message(
    std::uint8_t mode, std::string text) {
    const std::lock_guard lock(mutex_);
    local_messages_.push_back({mode, std::move(text)});
}

std::vector<CommunicationLocalMessage> CommunicationSessionMailbox::take_local_messages() {
    const std::lock_guard lock(mutex_);
    std::vector<CommunicationLocalMessage> messages;
    messages.swap(local_messages_);
    return messages;
}

void CommunicationSessionMailbox::enqueue_status_message(
    std::string text, bool persistent) {
    const std::lock_guard lock(mutex_);
    status_messages_.push_back({std::move(text), persistent});
}

std::vector<CommunicationStatusMessage> CommunicationSessionMailbox::take_status_messages() {
    const std::lock_guard lock(mutex_);
    std::vector<CommunicationStatusMessage> messages;
    messages.swap(status_messages_);
    return messages;
}

CommunicationRouteResult CommunicationSession::apply_speech(
    const network::State6SpeechMessage& message, std::time_t current_time) {
    if (uses_speaker_filters(message.mode) &&
        !talkbox_filters_.should_display_bubble(message.mode, message.speaker_name, -1))
        return {};
    if (message.mode == 0x10 || message.mode == 0x11)
        return {};

    std::int32_t identifier = -1;
    if (message.mode == 5 || message.mode == 10 || message.mode == 12 || message.mode == 14)
        identifier = message.mode_value_u16;
    const auto timestamp_offset = message.mode == 6
        ? std::bit_cast<std::int32_t>(0U - message.mode_value_u32) : 0;

    auto line = make_message_line(message.speaker_id, message.mode, message.speaker_name,
        message.level, message.text, timestamp_offset, current_time);
    return route(std::move(line), identifier);
}

CommunicationRouteResult CommunicationSession::apply_control(
    const network::State6CommunicationControl& update, std::time_t current_time) {
    if (update.opcode == 0xB2) {
        if (std::find(active_filter_channel_identifiers_.begin(),
                active_filter_channel_identifiers_.end(), update.field_u16) ==
            active_filter_channel_identifiers_.end())
            active_filter_channel_identifiers_.push_back(update.field_u16);
        return {};
    }
    if (update.opcode != 0xB4) return {};
    auto line = make_message_line(0, update.field_u8, {}, 0, update.text, 0, current_time);
    return route(std::move(line), -1);
}

void CommunicationSession::replace_available_channels(
    const std::vector<network::State6ChannelEntry>& channels) {
    available_channels_ = channels;
}

void CommunicationSession::open_channel(const network::State6ChannelOpen& channel) {
    const auto existing = std::find_if(open_channels_.begin(), open_channels_.end(),
        [&channel](const auto& entry) { return entry.id == channel.id; });
    if (existing == open_channels_.end()) open_channels_.push_back(channel);
    else *existing = channel;

    auto [message_buffer, inserted] = channel_message_buffers_.try_emplace(
        channel.id, 1000);
    (void)inserted;
    const auto buffer_exists = std::any_of(buffers_.begin(), buffers_.end(),
        [&channel](const auto& buffer) {
            return buffer.channel_identifier == channel.id;
        });
    if (!buffer_exists) {
        CommunicationBufferRoute route;
        route.lines = &message_buffer->second;
        route.mode_filter = 5;
        route.identifier_filter = static_cast<std::int32_t>(channel.id);
        route.active = true;
        route.channel_identifier = channel.id;
        buffers_.push_back(std::move(route));
    }

}

bool CommunicationSession::select_channel(std::uint16_t identifier) noexcept {
    auto channel = std::find_if(open_channels_.begin(), open_channels_.end(),
        [identifier](const auto& entry) { return entry.id == identifier; });
    if (channel == open_channels_.end()) return false;
    if (std::next(channel) != open_channels_.end()) {
        auto selected = std::move(*channel);
        open_channels_.erase(channel);
        open_channels_.push_back(std::move(selected));
    }
    channel_buffer_identifier_ = identifier;
    return true;
}

void CommunicationSession::select_default_channel() noexcept {
    channel_buffer_identifier_.reset();
}

bool CommunicationSession::toggle_server_messages(
    std::optional<std::uint16_t> identifier) noexcept {
    if (!identifier) {
        primary_server_messages_enabled_ = !primary_server_messages_enabled_;
        return primary_server_messages_enabled_;
    }
    const auto route = std::find_if(buffers_.begin(), buffers_.end(),
        [identifier](const auto& buffer) {
            return buffer.channel_identifier == identifier;
        });
    if (route == buffers_.end()) return false;
    route->server_messages_enabled = !route->server_messages_enabled;
    return route->server_messages_enabled;
}

bool CommunicationSession::server_messages_enabled(
    std::optional<std::uint16_t> identifier) const noexcept {
    if (!identifier) return primary_server_messages_enabled_;
    const auto route = std::find_if(buffers_.begin(), buffers_.end(),
        [identifier](const auto& buffer) {
            return buffer.channel_identifier == identifier;
        });
    return route != buffers_.end() && route->server_messages_enabled;
}

CommunicationRouteResult CommunicationSession::close_channel(
    std::uint16_t identifier, std::time_t current_time) {
    auto notice = make_message_line(0, 0x12, {}, 0,
        "The channel has been closed. You need to re-join the channel if you get invited.",
        0, current_time);
    CommunicationRouteResult result;
    const auto filter_identifier = static_cast<std::int32_t>(identifier);
    const auto message_buffer = channel_message_buffers_.find(identifier);
    if (message_buffer != channel_message_buffers_.end()) {
        for (std::size_t index = 0; index < buffers_.size(); ++index) {
            const auto& buffer = buffers_[index];
            if (buffer.channel_identifier != identifier || !buffer.active ||
                buffer.lines == nullptr || !matches_communication_buffer(
                    buffer, 5, {}, filter_identifier)) continue;
            if (buffer.lines->append(notice)) result.matching_buffers.push_back(index);
            break;
        }
    }
    std::erase_if(buffers_, [identifier](const auto& buffer) {
        return buffer.channel_identifier == identifier;
    });
    std::erase_if(open_channels_, [identifier](const auto& channel) {
        return channel.id == identifier;
    });
    std::erase(active_filter_channel_identifiers_, identifier);
    if (channel_buffer_identifier_ == identifier) {
        channel_buffer_identifier_.reset();
    }
    return result;
}

MessageLineList& CommunicationSession::primary_messages() noexcept {
    return primary_messages_;
}

const MessageLineList& CommunicationSession::primary_messages() const noexcept {
    return primary_messages_;
}

const std::vector<network::State6ChannelEntry>&
CommunicationSession::available_channels() const noexcept {
    return available_channels_;
}

const std::vector<network::State6ChannelOpen>&
CommunicationSession::open_channels() const noexcept {
    return open_channels_;
}

const MessageLineList& CommunicationSession::channel_messages() const noexcept {
    if (channel_buffer_identifier_) {
        const auto messages = channel_message_buffers_.find(*channel_buffer_identifier_);
        if (messages != channel_message_buffers_.end()) return messages->second;
    }
    return empty_channel_messages_;
}

const MessageLineList* CommunicationSession::channel_messages(
    std::uint16_t identifier) const noexcept {
    const auto messages = channel_message_buffers_.find(identifier);
    return messages == channel_message_buffers_.end() ? nullptr : &messages->second;
}

std::optional<std::uint16_t>
CommunicationSession::channel_buffer_identifier() const noexcept {
    return channel_buffer_identifier_;
}

bool CommunicationSession::has_active_private_channel_filter() const noexcept {
    return std::any_of(active_filter_channel_identifiers_.begin(),
        active_filter_channel_identifiers_.end(), [](std::uint16_t identifier) {
            return identifier >= 9;
        });
}

TalkboxFilterRegistry& CommunicationSession::talkbox_filters() noexcept {
    return talkbox_filters_;
}

CommunicationDisplayPreferences& CommunicationSession::display_preferences() noexcept {
    return display_preferences_;
}

std::string CommunicationSession::format_console_message(const MessageLine& message) const {
    return message.format(display_preferences_.timestamps_enabled,
        display_preferences_.levels_enabled);
}

bool CommunicationSession::should_display_talkbox(
    const network::State6SpeechMessage& message) const noexcept {
    if (!uses_speaker_filters(message.mode)) return true;
    return talkbox_filters_.should_display_bubble(message.mode, message.speaker_name, -1);
}

void CommunicationSession::replace_talkbox_filters(
    std::span<const std::string> white_list, std::span<const std::string> ignore_list) {
    std::vector<TalkboxSpeakerFilter> white_filters;
    std::vector<TalkboxSpeakerFilter> ignore_filters;
    white_filters.reserve(white_list.size());
    ignore_filters.reserve(ignore_list.size());
    for (const auto& name : white_list) white_filters.push_back({name, true});
    for (const auto& name : ignore_list) ignore_filters.push_back({name, true});
    replace_talkbox_filters(white_filters, ignore_filters);
}

void CommunicationSession::replace_talkbox_filters(
    std::span<const TalkboxSpeakerFilter> white_list,
    std::span<const TalkboxSpeakerFilter> ignore_list) {
    talkbox_filters_.clear(TalkboxFilterGroup::display_override);
    talkbox_filters_.clear(TalkboxFilterGroup::suppress_bubble);
    for (const auto& filter : white_list)
        talkbox_filters_.set_speaker_filter(TalkboxFilterGroup::display_override,
            filter.name, filter.active);
    for (const auto& filter : ignore_list)
        talkbox_filters_.set_speaker_filter(TalkboxFilterGroup::suppress_bubble,
            filter.name, filter.active);
}

std::vector<CommunicationBufferRoute>& CommunicationSession::buffers() noexcept {
    return buffers_;
}

CommunicationRoutingSettings& CommunicationSession::settings() noexcept {
    return settings_;
}

CommunicationRouteResult CommunicationSession::route(MessageLine message,
    std::int32_t identifier) {
    return route_communication_message(&primary_messages_, buffers_, std::move(message),
        identifier, settings_, primary_server_messages_enabled_);
}

}
