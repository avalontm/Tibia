#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tibia::network {

inline constexpr std::size_t tibia_receive_buffer_capacity = 0x400A;

// Reassembles 16-bit little-endian length-prefixed frames from arbitrary TCP
// read chunks. The returned frame includes its two-byte length prefix.
class TcpFrameBuffer {
public:
    explicit TcpFrameBuffer(std::size_t capacity = tibia_receive_buffer_capacity);
    bool append(std::span<const std::uint8_t> bytes);
    [[nodiscard]] bool has_frame() const noexcept;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> take_frame();
    [[nodiscard]] std::size_t buffered_size() const noexcept;

private:
    void compact();
    [[nodiscard]] std::size_t complete_frame_count() const noexcept;

    std::vector<std::uint8_t> bytes_;
    std::size_t begin_{};
    std::size_t capacity_;
};

} // namespace tibia::network
