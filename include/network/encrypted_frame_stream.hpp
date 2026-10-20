#pragma once

#include "crypto/xtea.hpp"
#include "network/encrypted_frame.hpp"
#include "network/tcp_frame_buffer.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace tibia::network {

// Combines TCP frame reassembly with the observed encrypted-frame decoder.
// Use after the login handshake has established the XTEA key.
class EncryptedFrameStream {
public:
    explicit EncryptedFrameStream(const crypto::XteaKey& key) noexcept;

    void append(std::span<const std::uint8_t> bytes);
    [[nodiscard]] std::optional<DecodedEncryptedFrame> next();
    [[nodiscard]] std::size_t buffered_size() const noexcept;

private:
    crypto::XteaKey key_;
    TcpFrameBuffer frames_;
};

} // namespace tibia::network
