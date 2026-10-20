#include "client/message_line_list.hpp"

#include <cstdio>
#include <cstdint>
#include <ctime>
#include <stdexcept>
#include <utility>

namespace tibia::client {
namespace {

std::uint8_t fold_case(std::uint8_t value) noexcept {
    if ((value >= 0x41 && value <= 0x5A) || (value >= 0xC1 && value <= 0xDE))
        return static_cast<std::uint8_t>(value + 0x20);
    return value;
}

bool equal_case_insensitive(const std::string& left, const std::string& right) noexcept {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (fold_case(static_cast<std::uint8_t>(left[i])) !=
            fold_case(static_cast<std::uint8_t>(right[i]))) return false;
    }
    return true;
}

}

std::string MessageLine::format(bool include_time, bool include_level) const {
    std::string result;
    if (include_time) result += time_prefix;
    result += speaker_name;
    if (include_level && !speaker_name.empty())
        result += speaker_level;
    if (!speaker_name.empty()) result += ": ";
    result += text;
    return result;
}

MessageLine make_message_line(std::uint32_t speaker_id, std::uint8_t mode,
    std::string_view speaker_name, std::int32_t speaker_level, std::string text,
    std::int32_t timestamp_offset_seconds, std::time_t current_time) {
    if (speaker_level < 0) throw std::invalid_argument("speaker level must be nonnegative");
    const auto timestamp = current_time + timestamp_offset_seconds;
    std::tm local{};
#if defined(_WIN32)
    if (localtime_s(&local, &timestamp) != 0)
        throw std::runtime_error("cannot convert message timestamp to local time");
#else
    if (localtime_r(&timestamp, &local) == nullptr)
        throw std::runtime_error("cannot convert message timestamp to local time");
#endif
    char time_prefix[7]{};
    const auto written = std::snprintf(time_prefix, sizeof(time_prefix), "%02d:%02d ",
        local.tm_hour, local.tm_min);
    if (written != 6) throw std::runtime_error("cannot format message timestamp");
    char level[8]{};
    const auto level_written = std::snprintf(level, sizeof(level), " [%d]", speaker_level);
    if (level_written < 0) throw std::runtime_error("cannot format speaker level");
    MessageLine line;
    line.speaker_id = speaker_id;
    line.mode = mode;
    line.time_prefix = time_prefix;
    line.speaker_name.assign(speaker_name.substr(0, 30));
    line.speaker_level = level;
    line.text = std::move(text);
    return line;
}

MessageLineList::MessageLineList(std::size_t capacity) : capacity_(capacity) {}

void MessageLineList::set_enabled(bool enabled) noexcept {
    enabled_ = enabled;
}

void MessageLineList::set_capacity(std::size_t capacity) {
    capacity_ = capacity;
    while (lines_.size() > capacity_) (void)remove(0);
}

bool MessageLineList::append(MessageLine line) {
    if (!enabled_) return false;
    const bool was_empty = lines_.empty();
    lines_.push_back(std::move(line));
    if (was_empty) empty_dirty_ = true;
    content_dirty_ = true;
    if (lines_.size() > capacity_) (void)remove(0);
    return true;
}

bool MessageLineList::append_unique(MessageLine line) {
    if (!enabled_) return false;
    std::size_t index = 0;
    while (index < lines_.size()) {
        if (equal_case_insensitive(lines_[index].speaker_name, line.speaker_name))
            (void)remove(index);
        else ++index;
    }
    return append(std::move(line));
}

bool MessageLineList::remove(std::size_t index) {
    if (index >= lines_.size()) return false;
    lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(index));
    if (cursor_ && *cursor_ == index) cursor_ = index < lines_.size()
        ? std::optional<std::size_t>(index) : std::nullopt;
    else if (cursor_ && *cursor_ > index) --*cursor_;
    if (!cursor_ && !lines_.empty()) cursor_ = 0;
    content_dirty_ = true;
    if (lines_.empty()) empty_dirty_ = true;
    return true;
}

void MessageLineList::clear() noexcept {
    const bool changed = !lines_.empty();
    lines_.clear();
    cursor_.reset();
    content_dirty_ = content_dirty_ || changed;
    empty_dirty_ = empty_dirty_ || changed;
}

bool MessageLineList::select(std::size_t index) noexcept {
    if (index >= lines_.size()) return false;
    cursor_ = index;
    return true;
}

bool MessageLineList::advance_cursor() noexcept {
    if (!cursor_ || *cursor_ >= lines_.size()) return false;
    ++*cursor_;
    if (*cursor_ >= lines_.size()) cursor_.reset();
    return cursor_.has_value();
}

std::optional<std::size_t> MessageLineList::cursor_index() const noexcept {
    return cursor_;
}

const std::deque<MessageLine>& MessageLineList::lines() const noexcept {
    return lines_;
}

bool MessageLineList::take_content_dirty() noexcept {
    const auto dirty = content_dirty_;
    content_dirty_ = false;
    return dirty;
}

bool MessageLineList::take_empty_dirty() noexcept {
    const auto dirty = empty_dirty_;
    empty_dirty_ = false;
    return dirty;
}

}
