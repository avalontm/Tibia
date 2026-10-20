#pragma once

#include "map/cache.hpp"
#include "map/model.hpp"
#include "resources/tibia_dat.hpp"
#include "resources/tibia_spr.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <list>
#include <optional>
#include <unordered_map>

namespace tibia::map {

inline constexpr std::size_t automap_view_size = 106;

struct AutomapView {
    std::array<resources::TibiaSprColor, automap_view_size * automap_view_size> pixels{};
    std::int32_t center_x{};
    std::int32_t center_y{};
    std::uint8_t floor{};
    std::int32_t player_pixel_x{};
    std::int32_t player_pixel_y{};
    std::int32_t zoom{2};
    bool player_visible{};
};

struct AutomapViewState {
    std::int32_t center_x{};
    std::int32_t center_y{};
    std::uint8_t floor{};
    std::int32_t reference_x{};
    std::int32_t reference_y{};
    std::uint8_t reference_floor{};
    std::int32_t zoom{2};
    bool follow_player{true};
};

enum class AutomapAction {
    zoom_in,
    zoom_out,
    higher_floor,
    lower_floor,
    center,
    north,
    east,
    south,
    west
};

void apply_automap_action(AutomapViewState& state, AutomapAction action,
    std::uint16_t player_x, std::uint16_t player_y,
    std::uint8_t player_floor) noexcept;
void update_automap_player_position(AutomapViewState& state,
    std::uint16_t player_x, std::uint16_t player_y,
    std::uint8_t player_floor) noexcept;

class AutomapStore {
public:
    AutomapStore();
    explicit AutomapStore(std::filesystem::path directory);
    ~AutomapStore();
    [[nodiscard]] std::optional<AutomapView> update(
        const DecodedMap& map, const resources::TibiaDatFile& dat,
        std::uint16_t player_x, std::uint16_t player_y, std::uint8_t player_floor,
        const AutomapViewState& state);
    [[nodiscard]] std::optional<std::vector<network::State6PathSearchCell>>
    path_search_grid(network::State6PlayerPosition origin);

private:
    [[nodiscard]] network::State6NativeMapCacheBlock* load_block(
        std::uint32_t block_x, std::uint32_t block_y, std::uint8_t floor);
    [[nodiscard]] bool save_block(network::State6NativeMapCacheBlock& block);

    std::filesystem::path directory_;
    struct CachedBlock {
        network::State6NativeMapCacheBlock block;
        std::list<std::uint64_t>::iterator lru_position;
        bool dirty{};
    };
    std::list<std::uint64_t> lru_;
    std::unordered_map<std::uint64_t, CachedBlock> blocks_;
};

[[nodiscard]] std::filesystem::path default_automap_directory();

}
