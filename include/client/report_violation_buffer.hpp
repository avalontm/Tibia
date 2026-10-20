#pragma once

#include "client/message_line_list.hpp"

#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace tibia::client {

struct ReportViolationBufferMessage {
    MessageLine line;
};

struct ReportViolationBufferSnapshot {
    bool open{};
    bool server_closed{};
    std::uint8_t type{7};
    std::string title{"Report Rule Violation"};
    std::vector<ReportViolationBufferMessage> messages;
};

class ReportViolationBuffer {
public:
    void open(std::string question, std::string player_name, std::uint32_t player_level);
    void append_speech_reply(std::uint32_t speaker_id, std::string speaker_name,
                             std::uint32_t speaker_level, std::string text,
                             std::time_t current_time);
    void mark_server_closed();
    void close();
    void clear();
    [[nodiscard]] bool is_open() const;
    [[nodiscard]] ReportViolationBufferSnapshot snapshot() const;

private:
    void append_message(std::uint8_t mode, std::string text,
                        std::string_view speaker_name = {}, std::uint32_t speaker_level = 0,
                        std::uint32_t speaker_id = 0, std::time_t current_time = 0);

    mutable std::mutex mutex_;
    bool open_{};
    bool server_closed_{};
    std::vector<ReportViolationBufferMessage> messages_;
};

}
