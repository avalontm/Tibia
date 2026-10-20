#pragma once

#include "network/server_catalog.hpp"

namespace tibia::network {

[[nodiscard]] std::array<char, legacy_server_address_capacity> copy_server_address(
    const char* address);

}
