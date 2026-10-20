#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace tibia::network {

class BoundedByteWriter {
public:
    explicit BoundedByteWriter(std::span<std::uint8_t> storage) noexcept;

    void reset() noexcept;
    void write_u8(std::uint8_t value);
    void write_u16(std::uint16_t value);
    void write_u32(std::uint32_t value);
    void write_c_string(const char* value);
    void write_bytes(std::span<const std::uint8_t> value);

    [[nodiscard]] std::size_t position() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::size_t remaining() const noexcept;

private:
    void require(std::size_t count) const;

    std::span<std::uint8_t> storage_;
    std::size_t position_{};
};

void write_command_prefix(BoundedByteWriter& writer, int command_type);

}
