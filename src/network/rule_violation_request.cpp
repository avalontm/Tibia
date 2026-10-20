#include "network/rule_violation_request.hpp"

#include "network/packet_stream.hpp"

#include <utility>

namespace tibia::network {

std::vector<std::uint8_t> encode_state6_rule_violation_request(
    std::string_view player_name, std::uint8_t reason_id,
    std::uint8_t action_id, std::string_view comment,
    std::uint32_t statement_id, std::uint8_t ip_banishment) {
    PacketWriter writer;
    writer.write_u8(0xe7);
    writer.write_string(player_name);
    writer.write_u8(reason_id);
    writer.write_u8(action_id);
    writer.write_string(comment);
    writer.write_u32(statement_id);
    writer.write_u8(ip_banishment);
    return std::move(writer).take();
}

}
