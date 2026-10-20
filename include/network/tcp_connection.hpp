#pragma once

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <memory>
#include <span>
#include <string_view>

namespace tibia::network {

// Blocking TCP transport. Connection destination and socket lifetime are
// explicit; packet framing remains the responsibility of TcpFrameBuffer.
class TcpConnection {
public:
    TcpConnection();
    ~TcpConnection();

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&&) noexcept;
    TcpConnection& operator=(TcpConnection&&) noexcept;

    void connect(std::string_view host, std::string_view service);
    void send_all(std::span<const std::uint8_t> bytes);
    // Waits until the socket is readable, returning false on timeout.
    [[nodiscard]] bool wait_for_data(std::chrono::milliseconds timeout);
    // Reads up to output.size() bytes. Returns zero when the peer has closed.
    [[nodiscard]] std::size_t receive_some(std::span<std::uint8_t> output);
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tibia::network
