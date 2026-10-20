#include "network/speech_request.hpp"

#include "network/packet_stream.hpp"

#include <utility>

namespace tibia::network {
namespace {

bool uses_addressee(std::uint8_t mode) noexcept {
    return mode == 4 || mode == 7 || mode == 11 || mode == 15;
}

bool uses_channel(std::uint8_t mode) noexcept {
    return mode == 5 || mode == 10 || mode == 14;
}

bool uses_text_only(std::uint8_t mode) noexcept {
    return mode == 1 || mode == 2 || mode == 3 || mode == 6 || mode == 8 ||
        mode == 9 || mode == 13;
}

}

std::optional<std::vector<std::uint8_t>> encode_state6_speech_request(
    std::uint8_t mode, std::string_view text, std::string_view addressee,
    std::optional<std::uint16_t> channel_id) {
    if (text.empty() ||
        (!uses_text_only(mode) && !uses_addressee(mode) && !uses_channel(mode)) ||
        (uses_addressee(mode) && addressee.empty()) ||
        (uses_channel(mode) && !channel_id))
        return std::nullopt;

    PacketWriter writer;
    writer.write_u8(0x96);
    writer.write_u8(mode);
    if (uses_addressee(mode)) writer.write_string(addressee);
    if (uses_channel(mode)) writer.write_u16(*channel_id);
    writer.write_string(text);
    return std::move(writer).take();
}

}
