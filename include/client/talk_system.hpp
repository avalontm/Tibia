#pragma once

#include "network/message_dispatcher.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tibia::client {

struct TalkboxLine {
    std::string text;
};

struct TalkboxColor {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
};

struct Talkbox {
    std::uint8_t state{};
    std::uint8_t mode{};
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint8_t floor{};
    TalkboxColor color{};
    std::uint32_t order{};
    std::int32_t remaining_ms{};
    std::vector<TalkboxLine> lines;
};

class TalkSystem {
public:
    static constexpr std::size_t talkbox_count = 10;
    static constexpr std::size_t line_count = 10;
    static constexpr std::size_t line_capacity = 40;

    void clear() noexcept;
    [[nodiscard]] bool apply(const network::State6SpeechMessage& message);
    [[nodiscard]] bool apply(const network::State6CommunicationControl& update);
    void set_status_message(std::optional<std::string_view> text, bool persistent);
    void advance(std::uint32_t elapsed_ms) noexcept;
    void advance_status_tick() noexcept;
    [[nodiscard]] const std::string& status_text() const noexcept;
    [[nodiscard]] std::int32_t status_ticks_remaining() const noexcept;
    [[nodiscard]] bool has_active_status_message() const noexcept;
    [[nodiscard]] bool take_status_dirty() noexcept;
    [[nodiscard]] const std::array<Talkbox, talkbox_count>& talkboxes() const noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;

private:
    [[nodiscard]] bool add_positioned_message(const network::State6SpeechMessage& message);
    [[nodiscard]] bool add_system_message(std::uint8_t mode, std::string_view text);
    [[nodiscard]] std::size_t oldest_occupied_index() const noexcept;
    static void append_lines(Talkbox& talkbox, std::string_view text);
    void select_next(std::size_t current) noexcept;

    std::array<Talkbox, talkbox_count> talkboxes_{};
    std::uint32_t next_order_{};
    std::string status_text_;
    std::int32_t status_ticks_remaining_{};
    bool status_dirty_{};
};

}
