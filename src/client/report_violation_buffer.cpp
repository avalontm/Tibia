#include "client/report_violation_buffer.hpp"

#include <ctime>
#include <utility>

namespace tibia::client {

void ReportViolationBuffer::open(std::string question, std::string player_name,
                                 std::uint32_t player_level) {
    const std::lock_guard lock(mutex_);
    if (open_) return;
    open_ = true;
    server_closed_ = false;
    append_message(8, std::move(question), player_name, player_level);
    append_message(0x12, "Please wait patiently for a gamemaster to reply.");
}

void ReportViolationBuffer::append_speech_reply(std::uint32_t speaker_id,
    std::string speaker_name, std::uint32_t speaker_level, std::string text,
    std::time_t current_time) {
    const std::lock_guard lock(mutex_);
    if (!open_) return;
    append_message(8, std::move(text), speaker_name, speaker_level, speaker_id,
        current_time);
}

void ReportViolationBuffer::mark_server_closed() {
    const std::lock_guard lock(mutex_);
    if (!open_ || server_closed_) return;
    append_message(0x12, "Your request has been closed.");
    server_closed_ = true;
}

void ReportViolationBuffer::close() {
    const std::lock_guard lock(mutex_);
    open_ = false;
    server_closed_ = false;
    messages_.clear();
}

void ReportViolationBuffer::clear() {
    close();
}

bool ReportViolationBuffer::is_open() const {
    const std::lock_guard lock(mutex_);
    return open_;
}

ReportViolationBufferSnapshot ReportViolationBuffer::snapshot() const {
    const std::lock_guard lock(mutex_);
    return {open_, server_closed_, 7, "Report Rule Violation", messages_};
}

void ReportViolationBuffer::append_message(std::uint8_t mode, std::string text,
    std::string_view speaker_name, std::uint32_t speaker_level,
    std::uint32_t speaker_id, std::time_t current_time) {
    constexpr std::size_t capacity = 1000;
    if (messages_.size() == capacity) messages_.erase(messages_.begin());
    if (current_time == 0) current_time = std::time(nullptr);
    auto line = make_message_line(speaker_id, mode, speaker_name,
        static_cast<std::int32_t>(speaker_level), std::move(text), 0, current_time);
    if (speaker_level == 0) line.speaker_level.clear();
    messages_.push_back({std::move(line)});
}

}
