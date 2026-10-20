#pragma once

#include "navigation/navigation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tibia::network {

inline constexpr std::size_t state6_native_map_block_bytes = 0x10000;

struct State6NativeMapCacheRecord {
    std::int32_t x{}, y{}, z{};
    std::array<std::uint8_t, 100> data{};
};

struct State6NativeMapCacheBlock {
    std::uint32_t block_x{}, block_y{};
    std::uint8_t floor{};
    std::array<std::uint8_t, state6_native_map_block_bytes> map_values{};
    std::array<std::uint8_t, state6_native_map_block_bytes> path_values{};
    std::vector<State6NativeMapCacheRecord> records;
};

[[nodiscard]] State6NativeMapCacheBlock make_empty_state6_native_map_cache_block(
    std::uint32_t block_x, std::uint32_t block_y, std::uint8_t floor) noexcept;

[[nodiscard]] std::optional<State6NativeMapCacheBlock>
decode_state6_native_map_cache_block(std::span<const std::uint8_t> bytes,
    std::uint32_t block_x, std::uint32_t block_y, std::uint8_t floor);

[[nodiscard]] std::optional<std::vector<State6PathSearchCell>>
make_state6_path_search_grid(State6PlayerPosition origin,
    std::span<const State6NativeMapCacheBlock> blocks);

}
