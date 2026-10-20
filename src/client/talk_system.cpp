#include "client/talk_system.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace tibia::client {
namespace {

bool is_line_break_character(unsigned char value) noexcept {
    return value == ' ' || value == '\f' || value == '\n' || value == '\r' ||
        value == '\t' || value == '\v';
}

bool is_talkbox_mode(std::uint8_t mode) noexcept {
    return mode == 1 || mode == 2 || mode == 3 || mode == 4 || mode == 9 ||
        mode == 0x0B || mode == 0x10 || mode == 0x11;
}

std::string heading_for(const network::State6SpeechMessage& message) {
    switch (message.mode) {
    case 1: return message.speaker_name + " says:";
    case 2: return message.speaker_name + " whispers:";
    case 3: return message.speaker_name + " yells:";
    case 4:
    case 9:
    case 0x0B: return message.speaker_name + ":";
    default: return {};
    }
}

TalkboxColor color_for_mode(std::uint8_t mode) noexcept {
    switch (mode) {
    case 1:
    case 2:
    case 3:
        return {0xF0, 0xF0, 0x00};
    case 4:
        return {0x60, 0xF8, 0xF8};
    case 9:
    case 0x0B:
        return {0xF8, 0x60, 0x60};
    case 0x10:
    case 0x11:
        return {0xFF, 0x66, 0x00};
    case 0x12:
        return {0xF8, 0x60, 0x60};
    case 0x13:
        return {0xF0, 0xF0, 0xF0};
    case 0x16:
        return {0x00, 0xF0, 0x00};
    default:
        return {};
    }
}

}

void TalkSystem::clear() noexcept {
    talkboxes_ = {};
    next_order_ = 0;
    status_text_.clear();
    status_ticks_remaining_ = 0;
    status_dirty_ = true;
}

bool TalkSystem::apply(const network::State6SpeechMessage& message) {
    if (!is_talkbox_mode(message.mode)) return false;
    return add_positioned_message(message);
}

bool TalkSystem::apply(const network::State6CommunicationControl& update) {
    if (update.opcode != 0xB4) return false;
    bool applied = false;
    if (update.field_u8 == 0x12 || update.field_u8 == 0x13 || update.field_u8 == 0x16)
        applied = add_system_message(update.field_u8, update.text);
    if (update.field_u8 != 0x14 && update.field_u8 != 0x15 && update.field_u8 != 0x17)
        return applied;
    set_status_message(update.text, false);
    return true;
}

void TalkSystem::set_status_message(std::optional<std::string_view> text,
    bool persistent) {
    status_dirty_ = true;
    if (!text) {
        status_ticks_remaining_ = 0;
        return;
    }
    const auto terminator = text->find('\0');
    const auto visible_text = text->substr(0, terminator);
    if (visible_text.size() >= 0x200)
        throw std::length_error("status message exceeds native limit");
    status_text_.assign(visible_text);
    status_ticks_remaining_ = persistent ? -1 : 50;
}

bool TalkSystem::add_system_message(std::uint8_t mode, std::string_view text) {
    if (mode == 0x16) {
        for (std::size_t i = 0; i < talkboxes_.size(); ++i) {
            if (talkboxes_[i].state != 0 && talkboxes_[i].mode == 0x16) select_next(i);
        }
    }

    auto free_slot = std::find_if(talkboxes_.begin(), talkboxes_.end(),
        [](const Talkbox& talkbox) { return talkbox.state == 0; });
    if (free_slot == talkboxes_.end()) {
        const auto oldest = oldest_occupied_index();
        if (oldest == talkboxes_.size()) return false;
        select_next(oldest);
        free_slot = std::find_if(talkboxes_.begin(), talkboxes_.end(),
            [](const Talkbox& talkbox) { return talkbox.state == 0; });
        if (free_slot == talkboxes_.end()) return false;
    }

    const auto index = static_cast<std::size_t>(free_slot - talkboxes_.begin());
    auto& current = talkboxes_[index];
    current = {};
    current.state = 1;
    current.mode = mode;
    current.color = color_for_mode(mode);
    current.order = next_order_++;
    current.remaining_ms = static_cast<std::int32_t>((text.size() / 2U + 30U) * 100U);
    if (mode == 0x12 || mode == 0x13) current.remaining_ms *= 2;
    append_lines(current, text);

    if (mode != 0x12 && mode != 0x13) return true;

    std::int32_t grouped_duration = 0;
    for (std::size_t other_index = 0; other_index < talkboxes_.size(); ++other_index) {
        if (other_index == index) continue;
        auto& other = talkboxes_[other_index];
        if (other.state == 0 || (other.mode != 0x13 && other.mode != 0x12 &&
            other.mode != 9 && other.mode != 0x0B)) continue;
        for (const auto& line : current.lines) {
            if (other.lines.size() == line_count) break;
            other.lines.push_back(line);
        }
        if (current.state == 1) current.remaining_ms /= 2;
        current.state = 2;
        other.state = 2;
        grouped_duration = std::max(grouped_duration, other.remaining_ms);
    }
    current.remaining_ms += grouped_duration;
    return true;
}

bool TalkSystem::add_positioned_message(const network::State6SpeechMessage& message) {
    auto free_slot = std::find_if(talkboxes_.begin(), talkboxes_.end(),
        [](const Talkbox& talkbox) { return talkbox.state == 0; });
    if (free_slot == talkboxes_.end()) {
        const auto oldest = oldest_occupied_index();
        if (oldest == talkboxes_.size()) return false;
        select_next(oldest);
        free_slot = std::find_if(talkboxes_.begin(), talkboxes_.end(),
            [](const Talkbox& talkbox) { return talkbox.state == 0; });
        if (free_slot == talkboxes_.end()) return false;
    }

    const auto index = static_cast<std::size_t>(free_slot - talkboxes_.begin());
    auto& current = talkboxes_[index];
    current = {};
    current.state = 1;
    current.mode = message.mode;
    current.color = color_for_mode(message.mode);
    current.x = message.has_position ? message.position.x : 0;
    current.y = message.has_position ? message.position.y : 0;
    current.floor = message.has_position ? message.position.floor : 0;
    current.order = next_order_++;
    current.remaining_ms = static_cast<std::int32_t>((message.text.size() / 2U + 30U) * 100U);

    append_lines(current, heading_for(message));
    append_lines(current, message.text);

    std::int32_t grouped_duration = 0;
    for (std::size_t other_index = 0; other_index < talkboxes_.size(); ++other_index) {
        if (other_index == index) continue;
        auto& other = talkboxes_[other_index];
        if (other.state == 0) continue;

        bool same_group = false;
        if (message.mode >= 1 && message.mode <= 3) {
            same_group = other.mode == message.mode && other.x == current.x &&
                other.y == current.y && other.floor == current.floor &&
                !other.lines.empty() && other.lines.front().text == heading_for(message);
        } else if (message.mode == 0x10 || message.mode == 0x11) {
            same_group = (other.mode == 0x10 || other.mode == 0x11) &&
                other.x == current.x && other.y == current.y && other.floor == current.floor;
        } else if (message.mode == 9 || message.mode == 0x0B) {
            same_group = other.mode == 0x13 || other.mode == 0x12 || other.mode == 9 || other.mode == 0x0B;
        } else if (message.mode == 4) {
            same_group = other.mode == 4;
        }

        if (!same_group) continue;
        const auto first_line = message.mode >= 1 && message.mode <= 3 ? 1U : 0U;
        for (std::size_t line_index = first_line; line_index < current.lines.size(); ++line_index) {
            if (other.lines.size() == line_count) break;
            other.lines.push_back(current.lines[line_index]);
        }
        if ((message.mode == 9 || message.mode == 0x0B || message.mode == 4) && other.state != 0) {
            if (current.state == 1) current.remaining_ms /= 2;
            other.state = 2;
            grouped_duration = std::max(grouped_duration, other.remaining_ms);
        }
        current.state = 2;
    }
    current.remaining_ms += grouped_duration;
    return true;
}

void TalkSystem::append_lines(Talkbox& talkbox, std::string_view text) {
    while (!text.empty() && talkbox.lines.size() < line_count) {
        const auto newline = text.find('\n');
        const auto limit = std::min(line_capacity - 1, newline == std::string_view::npos
            ? text.size() : newline);
        if (newline != std::string_view::npos && newline <= line_capacity - 1) {
            talkbox.lines.push_back({std::string(text.substr(0, newline))});
            text.remove_prefix(newline + 1);
            continue;
        }

        if (limit < text.size() && limit == line_capacity - 1) {
            std::size_t split = std::string_view::npos;
            for (std::size_t i = 0; i < line_capacity; ++i) {
                if (is_line_break_character(static_cast<unsigned char>(text[i]))) split = i;
            }
            if (split == std::string_view::npos) {
                talkbox.lines.push_back({std::string(text.substr(0, line_capacity - 2)) + "-"});
                text.remove_prefix(line_capacity - 2);
            } else {
                talkbox.lines.push_back({std::string(text.substr(0, split))});
                text.remove_prefix(split + 1);
            }
            continue;
        }

        talkbox.lines.push_back({std::string(text.substr(0, limit))});
        text.remove_prefix(limit);
        if (!text.empty() && text.front() == '\n') text.remove_prefix(1);
    }
}

std::size_t TalkSystem::oldest_occupied_index() const noexcept {
    std::size_t oldest = talkboxes_.size();
    for (std::size_t i = 0; i < talkboxes_.size(); ++i) {
        if (talkboxes_[i].state == 0) continue;
        if (oldest == talkboxes_.size() || talkboxes_[i].order < talkboxes_[oldest].order)
            oldest = i;
    }
    return oldest;
}

void TalkSystem::advance(std::uint32_t elapsed_ms) noexcept {
    for (std::size_t i = 0; i < talkboxes_.size(); ++i) {
        auto& talkbox = talkboxes_[i];
        if (talkbox.state == 0) continue;
        talkbox.remaining_ms -= static_cast<std::int32_t>(std::min<std::uint32_t>(
            elapsed_ms, static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())));
        if (talkbox.remaining_ms <= 0) select_next(i);
    }
}

void TalkSystem::advance_status_tick() noexcept {
    if (status_ticks_remaining_ <= 0) return;
    --status_ticks_remaining_;
    if (status_ticks_remaining_ == 0) status_dirty_ = true;
}

const std::string& TalkSystem::status_text() const noexcept {
    return status_text_;
}

std::int32_t TalkSystem::status_ticks_remaining() const noexcept {
    return status_ticks_remaining_;
}

bool TalkSystem::has_active_status_message() const noexcept {
    return status_ticks_remaining_ != 0;
}

bool TalkSystem::take_status_dirty() noexcept {
    const auto dirty = status_dirty_;
    status_dirty_ = false;
    return dirty;
}

void TalkSystem::select_next(std::size_t current) noexcept {
    auto& expired = talkboxes_[current];
    std::size_t candidate = talkboxes_.size();
    for (std::size_t i = 0; i < talkboxes_.size(); ++i) {
        if (i == current || talkboxes_[i].state != 2) continue;
        const auto& pending = talkboxes_[i];
        bool related = false;
        if (expired.mode >= 1 && expired.mode <= 3) {
            related = pending.mode == expired.mode && pending.x == expired.x &&
                pending.y == expired.y && pending.floor == expired.floor &&
                !pending.lines.empty() && !expired.lines.empty() &&
                pending.lines.front().text == expired.lines.front().text;
        } else if (expired.mode == 0x10 || expired.mode == 0x11) {
            related = (pending.mode == 0x10 || pending.mode == 0x11) &&
                pending.x == expired.x && pending.y == expired.y && pending.floor == expired.floor;
        } else if (expired.mode == 9 || expired.mode == 0x0B || expired.mode == 0x12 ||
            expired.mode == 0x13) {
            related = pending.mode == 9 || pending.mode == 0x0B || pending.mode == 0x12 ||
                pending.mode == 0x13;
        } else if (expired.mode == 4) {
            related = pending.mode == 4;
        }
        if (related && (candidate == talkboxes_.size() ||
            pending.order < talkboxes_[candidate].order))
            candidate = i;
    }
    if (candidate != talkboxes_.size()) talkboxes_[candidate].state = 1;
    expired = {};
}

const std::array<Talkbox, TalkSystem::talkbox_count>& TalkSystem::talkboxes() const noexcept {
    return talkboxes_;
}

std::size_t TalkSystem::active_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(talkboxes_.begin(), talkboxes_.end(),
        [](const Talkbox& talkbox) { return talkbox.state == 1; }));
}

}
