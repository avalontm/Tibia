#include "network/delta_patch.hpp"

#include <limits>
#include <stdexcept>

namespace tibia::network {
namespace {

std::uint32_t read_length(
    std::span<const std::uint8_t> stream,
    std::size_t& cursor,
    std::uint8_t encoded) {
    const auto tag = static_cast<std::uint8_t>(encoded & 0x3FU);
    if (tag <= 0x3CU) {
        return tag;
    }
    if (tag == 0x3DU) {
        if (cursor >= stream.size()) {
            throw std::invalid_argument("truncated 8-bit patch length");
        }
        return stream[cursor++];
    }
    if (tag == 0x3EU) {
        if (stream.size() - cursor < 2) {
            throw std::invalid_argument("truncated 16-bit patch length");
        }
        const auto value = static_cast<std::uint32_t>(stream[cursor]) |
            (static_cast<std::uint32_t>(stream[cursor + 1]) << 8U);
        cursor += 2;
        return value;
    }
    if (stream.size() - cursor < 4) {
        throw std::invalid_argument("truncated 32-bit patch length");
    }
    const auto value = static_cast<std::uint32_t>(stream[cursor]) |
        (static_cast<std::uint32_t>(stream[cursor + 1]) << 8U) |
        (static_cast<std::uint32_t>(stream[cursor + 2]) << 16U) |
        (static_cast<std::uint32_t>(stream[cursor + 3]) << 24U);
    cursor += 4;
    return value;
}

void require_available(std::size_t cursor, std::uint32_t count, std::size_t size, const char* what) {
    if (cursor > size || static_cast<std::uint64_t>(count) > size - cursor) {
        throw std::invalid_argument(what);
    }
}

} // namespace

std::vector<std::uint8_t> apply_observed_delta_patch(
    std::span<const std::uint8_t> base,
    std::span<const std::uint8_t> patch_stream,
    std::optional<std::uint32_t> expected_base_prefix) {
    // The fourth argument is compared with the first four bytes as a
    // little-endian word; zero disables the check in the original client.
    if (expected_base_prefix && *expected_base_prefix != 0) {
        if (base.size() < 4) {
            throw std::invalid_argument("base file is shorter than its signature");
        }
        const auto prefix = static_cast<std::uint32_t>(base[0]) |
            (static_cast<std::uint32_t>(base[1]) << 8U) |
            (static_cast<std::uint32_t>(base[2]) << 16U) |
            (static_cast<std::uint32_t>(base[3]) << 24U);
        if (prefix != *expected_base_prefix) {
            throw std::invalid_argument("base file signature does not match");
        }
    }

    std::vector<std::uint8_t> output;
    std::size_t base_cursor = 0;
    std::size_t patch_cursor = 0;

    while (patch_cursor < patch_stream.size()) {
        const auto command = patch_stream[patch_cursor++];
        const auto count = read_length(patch_stream, patch_cursor, command);
        switch (command & 0xC0U) {
        case 0x00U: // copy count bytes from the base file
            require_available(base_cursor, count, base.size(), "patch reads past the base file");
            output.insert(output.end(), base.begin() + static_cast<std::ptrdiff_t>(base_cursor),
                base.begin() + static_cast<std::ptrdiff_t>(base_cursor + count));
            base_cursor += count;
            break;
        case 0x40U: // skip count bytes in the base file
            require_available(base_cursor, count, base.size(), "patch skips past the base file");
            base_cursor += count;
            break;
        case 0x80U: // insert count literal bytes
            require_available(patch_cursor, count, patch_stream.size(), "truncated inserted patch data");
            output.insert(output.end(), patch_stream.begin() + static_cast<std::ptrdiff_t>(patch_cursor),
                patch_stream.begin() + static_cast<std::ptrdiff_t>(patch_cursor + count));
            patch_cursor += count;
            break;
        case 0xC0U: // replace count base bytes with count literal bytes
            require_available(base_cursor, count, base.size(), "patch replaces past the base file");
            require_available(patch_cursor, count, patch_stream.size(), "truncated replacement patch data");
            output.insert(output.end(), patch_stream.begin() + static_cast<std::ptrdiff_t>(patch_cursor),
                patch_stream.begin() + static_cast<std::ptrdiff_t>(patch_cursor + count));
            base_cursor += count;
            patch_cursor += count;
            break;
        }
    }

    return output;
}

} // namespace tibia::network
