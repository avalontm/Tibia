#include "network/server_address.hpp"

#include <cstring>
#include <stdexcept>

namespace tibia::network {

std::array<char, legacy_server_address_capacity> copy_server_address(const char* address) {
    if (address == nullptr) throw std::invalid_argument("server address must not be null");
    const auto length = std::strlen(address);
    if (length >= legacy_server_address_capacity)
        throw std::length_error("server address exceeds the native 99-byte limit");
    std::array<char, legacy_server_address_capacity> result{};
    std::memcpy(result.data(), address, length + 1);
    return result;
}

}
