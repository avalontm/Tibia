#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tibia::network {

// Small bounded codec for the client's observed little-endian packet fields.
class PacketWriter {
public:
    void write_u8(std::uint8_t value);
    void write_u16(std::uint16_t value);
    void write_u32(std::uint32_t value);
    void write_bytes(std::span<const std::uint8_t> value);
    void write_string(std::string_view value);

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept;
    [[nodiscard]] std::vector<std::uint8_t> take() && noexcept;

private:
    std::vector<std::uint8_t> bytes_;
};

class PacketReader {
public:
    explicit PacketReader(std::span<const std::uint8_t> bytes) noexcept;

    [[nodiscard]] std::uint8_t read_u8();
    [[nodiscard]] std::uint16_t read_u16();
    [[nodiscard]] std::uint32_t read_u32();
    [[nodiscard]] std::span<const std::uint8_t> read_bytes(std::size_t count);
    [[nodiscard]] std::string read_string();
    [[nodiscard]] std::size_t position() const noexcept;
    [[nodiscard]] std::size_t remaining() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> remaining_bytes() const noexcept;

private:
    void require(std::size_t count) const;

    std::span<const std::uint8_t> bytes_;
    std::size_t position_{};
};

} // namespace tibia::network
