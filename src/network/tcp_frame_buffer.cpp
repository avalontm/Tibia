#include "network/tcp_frame_buffer.hpp"

#include <algorithm>
#include <stdexcept>

namespace tibia::network {

TcpFrameBuffer::TcpFrameBuffer(std::size_t capacity) : capacity_(capacity) {
    if (capacity_ < 2) {
        throw std::invalid_argument("TCP frame buffer capacity must hold a frame prefix");
    }
}

bool TcpFrameBuffer::append(std::span<const std::uint8_t> bytes) {
    const auto complete_before = complete_frame_count();
    if (begin_ != 0) {
        compact();
    }
    if (bytes.size() > capacity_ - bytes_.size()) {
        throw std::length_error("TCP receive buffer capacity exceeded");
    }
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    return complete_frame_count() > complete_before;
}

bool TcpFrameBuffer::has_frame() const noexcept {
    const auto available = bytes_.size() - begin_;
    if (available < 2) {
        return false;
    }
    const auto payload_length = static_cast<std::size_t>(bytes_[begin_]) |
                                (static_cast<std::size_t>(bytes_[begin_ + 1]) << 8U);
    return available >= payload_length + 2U;
}

std::optional<std::vector<std::uint8_t>> TcpFrameBuffer::take_frame() {
    if (!has_frame()) {
        return std::nullopt;
    }
    const auto payload_length = static_cast<std::size_t>(bytes_[begin_]) |
                                (static_cast<std::size_t>(bytes_[begin_ + 1]) << 8U);
    const auto frame_length = payload_length + 2U;
    std::vector<std::uint8_t> frame(
        bytes_.begin() + static_cast<std::ptrdiff_t>(begin_),
        bytes_.begin() + static_cast<std::ptrdiff_t>(begin_ + frame_length));
    begin_ += frame_length;
    if (begin_ == bytes_.size()) {
        bytes_.clear();
        begin_ = 0;
    } else if (begin_ >= bytes_.size() / 2) {
        compact();
    }
    return frame;
}

std::size_t TcpFrameBuffer::buffered_size() const noexcept {
    return bytes_.size() - begin_;
}

std::size_t TcpFrameBuffer::complete_frame_count() const noexcept {
    auto cursor = begin_;
    std::size_t count = 0;
    while (bytes_.size() - cursor >= 2) {
        const auto payload_length = static_cast<std::size_t>(bytes_[cursor]) |
                                    (static_cast<std::size_t>(bytes_[cursor + 1]) << 8U);
        const auto frame_length = payload_length + 2U;
        if (frame_length > bytes_.size() - cursor) {
            break;
        }
        cursor += frame_length;
        ++count;
    }
    return count;
}

void TcpFrameBuffer::compact() {
    if (begin_ == 0) {
        return;
    }
    std::move(bytes_.begin() + static_cast<std::ptrdiff_t>(begin_), bytes_.end(), bytes_.begin());
    bytes_.resize(bytes_.size() - begin_);
    begin_ = 0;
}

} // namespace tibia::network
