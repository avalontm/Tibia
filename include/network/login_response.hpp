#pragma once

#include "network/message_dispatcher.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tibia::network {

struct CharacterEntry {
    std::string name;
    std::string world;
    std::array<std::uint8_t, 4> server_ipv4{};
    std::uint16_t server_port{};
};

struct CharacterListResponse {
    std::vector<CharacterEntry> characters;
    // The original stores this trailing word in a separate status field;
    // keep its meaning opaque until its consumer is fully traced.
    std::uint16_t account_status{};
};

// State 4, opcode 0x64: count byte, then name/world strings (each copied into
// a 30-byte native buffer), four IPv4 bytes and a little-endian port per row,
// followed by one opaque 16-bit account-status value.
[[nodiscard]] CharacterListResponse read_observed_character_list(PacketReader& reader);

using CharacterListHandler = std::function<void(const CharacterListResponse&)>;

void register_observed_character_list_handler(
    MessageDispatcher& dispatcher, CharacterListHandler consume_character_list);

} // namespace tibia::network
