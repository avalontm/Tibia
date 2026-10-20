#include "network/encrypted_frame_stream.hpp"

#include <utility>

namespace tibia::network {

EncryptedFrameStream::EncryptedFrameStream(const crypto::XteaKey& key) noexcept : key_(key) {}

void EncryptedFrameStream::append(std::span<const std::uint8_t> bytes) {
    frames_.append(bytes);
}

std::optional<DecodedEncryptedFrame> EncryptedFrameStream::next() {
    auto frame = frames_.take_frame();
    if (!frame) {
        return std::nullopt;
    }
    return decode_encrypted_frame(*frame, key_);
}

std::size_t EncryptedFrameStream::buffered_size() const noexcept {
    return frames_.buffered_size();
}

} // namespace tibia::network
