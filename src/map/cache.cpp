#include "map/cache.hpp"

#include <algorithm>
#include <bit>

namespace tibia::network {

State6NativeMapCacheBlock make_empty_state6_native_map_cache_block(
    std::uint32_t block_x, std::uint32_t block_y, std::uint8_t floor) noexcept {
    State6NativeMapCacheBlock block;
    block.block_x = block_x;
    block.block_y = block_y;
    block.floor = floor;
    block.map_values.fill(0);
    block.path_values.fill(0xFA);
    return block;
}

std::optional<State6NativeMapCacheBlock> decode_state6_native_map_cache_block(
    std::span<const std::uint8_t> bytes, std::uint32_t block_x,
    std::uint32_t block_y, std::uint8_t floor) {
    constexpr std::size_t arrays_size = state6_native_map_block_bytes * 2;
    constexpr std::size_t record_size = 12 + 100;
    if (bytes.size() < arrays_size) return std::nullopt;

    auto block = make_empty_state6_native_map_cache_block(block_x, block_y, floor);
    std::copy_n(bytes.begin(), state6_native_map_block_bytes, block.map_values.begin());
    std::copy_n(bytes.begin() + state6_native_map_block_bytes,
        state6_native_map_block_bytes, block.path_values.begin());
    if (bytes.size() == arrays_size) return block;
    if (bytes.size() - arrays_size < sizeof(std::uint32_t)) return std::nullopt;

    const auto read_u32 = [&](std::size_t offset) noexcept {
        return static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    };
    const auto record_count = read_u32(arrays_size);
    const auto payload_size = bytes.size() - arrays_size - sizeof(std::uint32_t);
    if (record_count > payload_size / record_size) return std::nullopt;

    block.records.reserve(record_count);
    auto offset = arrays_size + sizeof(std::uint32_t);
    for (std::uint32_t i = 0; i < record_count; ++i) {
        State6NativeMapCacheRecord record;
        record.x = std::bit_cast<std::int32_t>(read_u32(offset));
        record.y = std::bit_cast<std::int32_t>(read_u32(offset + 4));
        record.z = std::bit_cast<std::int32_t>(read_u32(offset + 8));
        std::copy_n(bytes.begin() + offset + 12, record.data.size(), record.data.begin());
        block.records.push_back(record);
        offset += record_size;
    }
    return block;
}

std::optional<std::vector<State6PathSearchCell>> make_state6_path_search_grid(
    State6PlayerPosition origin, std::span<const State6NativeMapCacheBlock> blocks) {
    constexpr std::int32_t radius = 110;
    constexpr std::size_t width = radius * 2U + 1U;
    if (origin.z > 15) return std::nullopt;

    std::vector<State6PathSearchCell> grid(width * width);
    for (std::int32_t dy = -radius; dy <= radius; ++dy) {
        for (std::int32_t dx = -radius; dx <= radius; ++dx) {
            const auto world_x = static_cast<std::int64_t>(origin.x) + dx;
            const auto world_y = static_cast<std::int64_t>(origin.y) + dy;
            const auto output_index = static_cast<std::size_t>(dy + radius) * width +
                static_cast<std::size_t>(dx + radius);
            if (world_x < 0 || world_y < 0) {
                grid[output_index] = {true, 1};
                continue;
            }

            const auto block_x = static_cast<std::uint32_t>(world_x >> 8);
            const auto block_y = static_cast<std::uint32_t>(world_y >> 8);
            const auto found = std::find_if(blocks.begin(), blocks.end(),
                [&](const State6NativeMapCacheBlock& block) {
                    return block.block_x == block_x && block.block_y == block_y &&
                        block.floor == origin.z;
                });
            if (found == blocks.end()) return std::nullopt;
            const auto local_x = static_cast<std::size_t>(world_x & 0xFF);
            const auto local_y = static_cast<std::size_t>(world_y & 0xFF);
            const auto raw_value = found->path_values[local_y * 256 + local_x];
            grid[output_index] = {raw_value == 0xFF, raw_value};
        }
    }
    return grid;
}

}
