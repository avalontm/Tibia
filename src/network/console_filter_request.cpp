#include "network/console_filter_request.hpp"

#include "network/packet_stream.hpp"

#include <utility>

namespace tibia::network {

std::optional<std::vector<std::uint8_t>> encode_state6_console_filter_request(
    State6ConsoleFilterOperation operation, std::string_view name) {
    if (name.empty()) return std::nullopt;

    std::uint8_t opcode{};
    switch (operation) {
    case State6ConsoleFilterOperation::invite:
        opcode = 0xAB;
        break;
    case State6ConsoleFilterOperation::exclude:
        opcode = 0xAC;
        break;
    default:
        return std::nullopt;
    }

    PacketWriter writer;
    writer.write_u8(opcode);
    writer.write_string(name);
    return std::move(writer).take();
}

}
