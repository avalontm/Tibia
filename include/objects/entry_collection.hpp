#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace tibia::objects {

class EntryCollectionView {
public:
    static constexpr std::size_t record_size = 0x4c;

    EntryCollectionView(int first_index, std::span<const std::uint8_t> records);

    [[nodiscard]] std::span<const std::uint8_t> at(int index) const;
    [[nodiscard]] std::uint32_t read_u32(int index, std::size_t field_offset) const;
    [[nodiscard]] int first_index() const noexcept;
    [[nodiscard]] int last_index() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    int first_index_;
    std::span<const std::uint8_t> records_;
};

}
