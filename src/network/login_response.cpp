#include "network/login_response.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace tibia::network {
namespace {

std::string read_native_character_string(PacketReader& reader) {
    auto value = reader.read_string();
    // FUN_004E35A0 passes a 30-byte destination to the shared string reader;
    // that reader consumes the complete wire string but copies at most 29 bytes.
    if (value.size() > 29) value.resize(29);
    const auto nul = value.find('\0');
    if (nul != std::string::npos) value.resize(nul);
    return value;
}

} // namespace

CharacterListResponse read_observed_character_list(PacketReader& reader) {
    CharacterListResponse response;
    const auto count = reader.read_u8();
    response.characters.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        CharacterEntry character;
        character.name = read_native_character_string(reader);
        character.world = read_native_character_string(reader);
        const auto address = reader.read_bytes(character.server_ipv4.size());
        std::copy(address.begin(), address.end(), character.server_ipv4.begin());
        character.server_port = reader.read_u16();
        response.characters.push_back(std::move(character));
    }
    response.account_status = reader.read_u16();
    return response;
}

void register_observed_character_list_handler(
    MessageDispatcher& dispatcher, CharacterListHandler consume_character_list) {
    if (!consume_character_list)
        throw std::invalid_argument("character-list consumer must be callable");
    dispatcher.register_handler(DispatchPhase::state4, 0x64,
        [consumer = std::move(consume_character_list)](PacketReader& reader) {
            consumer(read_observed_character_list(reader));
        });
}

} // namespace tibia::network
