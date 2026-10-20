#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace tibia::objects {

class EntryAttributeFlags {
public:
    explicit EntryAttributeFlags(std::span<const std::uint8_t> bytes) noexcept;

    [[nodiscard]] bool contains(std::size_t index) const;

private:
    std::span<const std::uint8_t> bytes_;
};

}
