#pragma once

#include <cstddef>
#include <deque>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <ctime>

namespace tibia::client {

struct MessageLine {
    std::uint32_t speaker_id{};
    std::uint8_t mode{};
    std::string time_prefix;
    std::string speaker_name;
    std::string speaker_level;
    std::string text;

    [[nodiscard]] std::string format(bool include_time, bool include_level) const;
};

[[nodiscard]] MessageLine make_message_line(std::uint32_t speaker_id,
    std::uint8_t mode, std::string_view speaker_name, std::int32_t speaker_level,
    std::string text, std::int32_t timestamp_offset_seconds, std::time_t current_time);

class MessageLineList {
public:
    explicit MessageLineList(std::size_t capacity);

    void set_enabled(bool enabled) noexcept;
    void set_capacity(std::size_t capacity);
    [[nodiscard]] bool append(MessageLine line);
    [[nodiscard]] bool append_unique(MessageLine line);
    [[nodiscard]] bool remove(std::size_t index);
    void clear() noexcept;
    [[nodiscard]] bool select(std::size_t index) noexcept;
    [[nodiscard]] bool advance_cursor() noexcept;
    [[nodiscard]] std::optional<std::size_t> cursor_index() const noexcept;
    [[nodiscard]] const std::deque<MessageLine>& lines() const noexcept;
    [[nodiscard]] bool take_content_dirty() noexcept;
    [[nodiscard]] bool take_empty_dirty() noexcept;

private:
    std::deque<MessageLine> lines_;
    std::optional<std::size_t> cursor_;
    std::size_t capacity_{};
    bool enabled_{true};
    bool content_dirty_{};
    bool empty_dirty_{};
};

}
