#include "network/tcp_connection.hpp"

#ifndef _WIN32
#error "The reconstructed TCP transport currently targets Windows Winsock."
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

namespace tibia::network {

struct TcpConnection::Impl {
    WSADATA winsock_data{};
    SOCKET socket{INVALID_SOCKET};
    bool winsock_started{};
    mutable std::mutex socket_mutex;

    [[nodiscard]] SOCKET socket_snapshot() const noexcept {
        const std::lock_guard lock(socket_mutex);
        return socket;
    }

    Impl() {
        const int result = WSAStartup(MAKEWORD(2, 2), &winsock_data);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed with code " + std::to_string(result));
        }
        winsock_started = true;
    }

    ~Impl() {
        close_socket();
        if (winsock_started) {
            WSACleanup();
        }
    }

    void close_socket() noexcept {
        SOCKET closing = INVALID_SOCKET;
        {
            const std::lock_guard lock(socket_mutex);
            closing = std::exchange(socket, INVALID_SOCKET);
        }
        if (closing != INVALID_SOCKET) {
            shutdown(closing, SD_BOTH);
            closesocket(closing);
        }
    }
};

TcpConnection::TcpConnection() : impl_(std::make_unique<Impl>()) {}
TcpConnection::~TcpConnection() = default;
TcpConnection::TcpConnection(TcpConnection&&) noexcept = default;
TcpConnection& TcpConnection::operator=(TcpConnection&&) noexcept = default;

void TcpConnection::connect(std::string_view host, std::string_view service) {
    if (!impl_) {
        throw std::logic_error("TCP connection object was moved from");
    }
    if (impl_->socket_snapshot() != INVALID_SOCKET) {
        throw std::logic_error("TCP connection is already open");
    }
    const std::string host_string(host);
    const std::string service_string(service);

    ADDRINFOA hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    ADDRINFOA* addresses = nullptr;
    const int lookup = getaddrinfo(host_string.c_str(), service_string.c_str(), &hints, &addresses);
    if (lookup != 0) {
        throw std::runtime_error("getaddrinfo failed with code " + std::to_string(lookup));
    }

    int last_error = 0;
    for (auto* address = addresses; address != nullptr; address = address->ai_next) {
        const SOCKET candidate = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate == INVALID_SOCKET) {
            last_error = WSAGetLastError();
            continue;
        }
        u_long nonblocking = 1;
        if (ioctlsocket(candidate, FIONBIO, &nonblocking) == SOCKET_ERROR) {
            last_error = WSAGetLastError();
            closesocket(candidate);
            continue;
        }
        int connect_result = ::connect(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen));
        if (connect_result == SOCKET_ERROR) {
            last_error = WSAGetLastError();
            if (last_error == WSAEWOULDBLOCK || last_error == WSAEINPROGRESS ||
                last_error == WSAEINVAL) {
                fd_set writable{};
                fd_set exceptional{};
                FD_ZERO(&writable);
                FD_ZERO(&exceptional);
                FD_SET(candidate, &writable);
                FD_SET(candidate, &exceptional);
                timeval timeout{8, 0};
                const int ready = ::select(0, nullptr, &writable, &exceptional, &timeout);
                if (ready > 0 && FD_ISSET(candidate, &writable) &&
                    !FD_ISSET(candidate, &exceptional)) {
                    int socket_error = 0;
                    int option_size = sizeof(socket_error);
                    if (getsockopt(candidate, SOL_SOCKET, SO_ERROR,
                            reinterpret_cast<char*>(&socket_error), &option_size) == 0 &&
                        socket_error == 0) {
                        connect_result = 0;
                    } else {
                        last_error = socket_error != 0 ? socket_error : WSAGetLastError();
                    }
                } else if (ready == 0) {
                    last_error = WSAETIMEDOUT;
                } else if (ready == SOCKET_ERROR) {
                    last_error = WSAGetLastError();
                }
            }
        }
        if (connect_result == 0) {
            nonblocking = 0;
            if (ioctlsocket(candidate, FIONBIO, &nonblocking) == SOCKET_ERROR) {
                last_error = WSAGetLastError();
                closesocket(candidate);
                continue;
            }
            {
                const std::lock_guard lock(impl_->socket_mutex);
                impl_->socket = candidate;
            }
            break;
        }
        last_error = WSAGetLastError();
        closesocket(candidate);
    }
    freeaddrinfo(addresses);
    if (impl_->socket_snapshot() == INVALID_SOCKET) {
        throw std::runtime_error("TCP connect failed with Winsock code " + std::to_string(last_error));
    }
}

void TcpConnection::send_all(std::span<const std::uint8_t> bytes) {
    const auto socket = impl_ ? impl_->socket_snapshot() : INVALID_SOCKET;
    if (socket == INVALID_SOCKET) {
        throw std::logic_error("TCP connection is not open");
    }
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const auto chunk = (std::min)(bytes.size() - sent,
                                      static_cast<std::size_t>((std::numeric_limits<int>::max)()));
        const int result = ::send(socket, reinterpret_cast<const char*>(bytes.data() + sent),
                                  static_cast<int>(chunk), 0);
        if (result == SOCKET_ERROR) {
            throw std::runtime_error("TCP send failed with Winsock code " +
                                     std::to_string(WSAGetLastError()));
        }
        if (result == 0) {
            throw std::runtime_error("TCP send returned zero before the buffer was complete");
        }
        sent += static_cast<std::size_t>(result);
    }
}

bool TcpConnection::wait_for_data(std::chrono::milliseconds timeout) {
    const auto socket = impl_ ? impl_->socket_snapshot() : INVALID_SOCKET;
    if (socket == INVALID_SOCKET) {
        throw std::logic_error("TCP connection is not open");
    }
    if (timeout.count() < 0) {
        throw std::invalid_argument("socket wait timeout cannot be negative");
    }
    fd_set readable{};
    FD_ZERO(&readable);
    FD_SET(socket, &readable);
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(timeout);
    timeval tv{};
    tv.tv_sec = static_cast<long>(micros.count() / 1'000'000);
    tv.tv_usec = static_cast<long>(micros.count() % 1'000'000);
    const int result = ::select(0, &readable, nullptr, nullptr, &tv);
    if (result == SOCKET_ERROR) {
        const auto error = WSAGetLastError();
        if (error == WSAEINTR) return false;
        throw std::runtime_error("TCP select failed with Winsock code " + std::to_string(error));
    }
    return result > 0;
}

std::size_t TcpConnection::receive_some(std::span<std::uint8_t> output) {
    const auto socket = impl_ ? impl_->socket_snapshot() : INVALID_SOCKET;
    if (socket == INVALID_SOCKET) {
        throw std::logic_error("TCP connection is not open");
    }
    if (output.empty()) {
        return 0;
    }
    const auto chunk = (std::min)(output.size(),
                                  static_cast<std::size_t>((std::numeric_limits<int>::max)()));
    const int result = ::recv(socket, reinterpret_cast<char*>(output.data()),
                              static_cast<int>(chunk), 0);
    if (result == SOCKET_ERROR) {
        throw std::runtime_error("TCP receive failed with Winsock code " +
                                 std::to_string(WSAGetLastError()));
    }
    return static_cast<std::size_t>(result);
}

void TcpConnection::close() noexcept {
    if (impl_) {
        impl_->close_socket();
    }
}

bool TcpConnection::is_open() const noexcept {
    return impl_ && impl_->socket_snapshot() != INVALID_SOCKET;
}

} // namespace tibia::network
