#include "map/automap_store.hpp"

#include "map/tile_object_order.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <limits>
#include <vector>

namespace tibia::map {
namespace {

std::uint64_t block_key(std::uint32_t x, std::uint32_t y, std::uint8_t floor) noexcept {
    return (static_cast<std::uint64_t>(floor) << 48) |
        (static_cast<std::uint64_t>(x) << 24) | y;
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

resources::TibiaSprColor automap_color(std::uint8_t index) noexcept {
    return {
        static_cast<std::uint8_t>((index / 36U) * 51U),
        static_cast<std::uint8_t>(((index / 6U) % 6U) * 51U),
        static_cast<std::uint8_t>((index % 6U) * 51U)
    };
}

}

void update_automap_player_position(AutomapViewState& state,
    std::uint16_t player_x, std::uint16_t player_y,
    std::uint8_t player_floor) noexcept {
    if (!state.follow_player && (state.reference_x != player_x ||
        state.reference_y != player_y || state.reference_floor != player_floor))
        state.follow_player = true;
    if (state.follow_player) {
        state.center_x = player_x;
        state.center_y = player_y;
        state.floor = player_floor;
    }
}

void apply_automap_action(AutomapViewState& state, AutomapAction action,
    std::uint16_t player_x, std::uint16_t player_y,
    std::uint8_t player_floor) noexcept {
    if (action == AutomapAction::zoom_in) {
        if (state.zoom < 8) state.zoom *= 2;
        return;
    }
    if (action == AutomapAction::zoom_out) {
        if (state.zoom > 1) state.zoom /= 2;
        return;
    }
    if (action == AutomapAction::center) {
        state.follow_player = true;
        update_automap_player_position(state, player_x, player_y, player_floor);
        return;
    }
    if (state.follow_player) {
        state.center_x = player_x;
        state.center_y = player_y;
        state.floor = player_floor;
        state.reference_x = player_x;
        state.reference_y = player_y;
        state.reference_floor = player_floor;
        state.follow_player = false;
    }
    switch (action) {
    case AutomapAction::higher_floor:
        if (state.floor > 0) --state.floor;
        break;
    case AutomapAction::lower_floor:
        if (state.floor < 15) ++state.floor;
        break;
    case AutomapAction::north:
        state.center_y -= 24 / state.zoom;
        break;
    case AutomapAction::east:
        state.center_x += 24 / state.zoom;
        break;
    case AutomapAction::south:
        state.center_y += 24 / state.zoom;
        break;
    case AutomapAction::west:
        state.center_x -= 24 / state.zoom;
        break;
    default:
        break;
    }
}

std::filesystem::path default_automap_directory() {
    std::array<wchar_t, MAX_PATH> app_data_path{};
    const auto result = SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
        nullptr, SHGFP_TYPE_CURRENT, app_data_path.data());
    if (FAILED(result)) return {};
    return std::filesystem::path(app_data_path.data()) / L"Tibia" / L"Automap";
}

AutomapStore::AutomapStore()
    : AutomapStore(default_automap_directory()) {}

AutomapStore::AutomapStore(std::filesystem::path directory)
    : directory_(std::move(directory)) {}

AutomapStore::~AutomapStore() {
    for (auto& [key, cached] : blocks_) {
        (void)key;
        if (cached.dirty) (void)save_block(cached.block);
    }
}

network::State6NativeMapCacheBlock* AutomapStore::load_block(
    std::uint32_t block_x, std::uint32_t block_y, std::uint8_t floor) {
    const auto key = block_key(block_x, block_y, floor);
    if (auto found = blocks_.find(key); found != blocks_.end()) {
        lru_.splice(lru_.begin(), lru_, found->second.lru_position);
        found->second.lru_position = lru_.begin();
        return &found->second.block;
    }

    if (blocks_.size() == 10) {
        const auto evicted_key = lru_.back();
        if (auto evicted = blocks_.find(evicted_key); evicted != blocks_.end()) {
            if (evicted->second.dirty) (void)save_block(evicted->second.block);
            blocks_.erase(evicted);
        }
        lru_.pop_back();
    }

    std::array<char, 16> filename{};
    std::snprintf(filename.data(), filename.size(), "%03u%03u%02u.map",
        block_x, block_y, static_cast<unsigned>(floor));
    auto block = network::make_empty_state6_native_map_cache_block(
        block_x, block_y, floor);
    std::ifstream input(directory_ / filename.data(), std::ios::binary);
    if (input) {
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
        if (auto decoded = network::decode_state6_native_map_cache_block(
                bytes, block_x, block_y, floor))
            block = std::move(*decoded);
    }
    lru_.push_front(key);
    auto [entry, inserted] = blocks_.emplace(key,
        CachedBlock{std::move(block), lru_.begin(), false});
    (void)inserted;
    return &entry->second.block;
}

bool AutomapStore::save_block(network::State6NativeMapCacheBlock& block) {
    if (directory_.empty()) return false;
    std::array<char, 16> filename{};
    std::snprintf(filename.data(), filename.size(), "%03u%03u%02u.map",
        block.block_x, block.block_y, static_cast<unsigned>(block.floor));
    std::vector<std::uint8_t> bytes;
    bytes.reserve(network::state6_native_map_block_bytes * 2 + 4 +
        block.records.size() * 112);
    bytes.insert(bytes.end(), block.map_values.begin(), block.map_values.end());
    bytes.insert(bytes.end(), block.path_values.begin(), block.path_values.end());
    append_u32(bytes, static_cast<std::uint32_t>(block.records.size()));
    for (const auto& record : block.records) {
        append_u32(bytes, static_cast<std::uint32_t>(record.x));
        append_u32(bytes, static_cast<std::uint32_t>(record.y));
        append_u32(bytes, static_cast<std::uint32_t>(record.z));
        bytes.insert(bytes.end(), record.data.begin(), record.data.end());
    }
    std::error_code directory_error;
    std::filesystem::create_directories(directory_, directory_error);
    if (directory_error) return false;
    std::ofstream output(directory_ / filename.data(),
        std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return output.good();
}

std::optional<std::vector<network::State6PathSearchCell>>
AutomapStore::path_search_grid(network::State6PlayerPosition origin) {
    constexpr std::int64_t radius = 110;
    if (origin.z > 15) return std::nullopt;
    const auto minimum_x = std::max<std::int64_t>(0,
        static_cast<std::int64_t>(origin.x) - radius);
    const auto minimum_y = std::max<std::int64_t>(0,
        static_cast<std::int64_t>(origin.y) - radius);
    const auto maximum_x = static_cast<std::int64_t>(origin.x) + radius;
    const auto maximum_y = static_cast<std::int64_t>(origin.y) + radius;

    std::vector<network::State6NativeMapCacheBlock> blocks;
    for (auto block_y = static_cast<std::uint32_t>(minimum_y) >> 8;
         block_y <= static_cast<std::uint32_t>(maximum_y) >> 8; ++block_y) {
        for (auto block_x = static_cast<std::uint32_t>(minimum_x) >> 8;
             block_x <= static_cast<std::uint32_t>(maximum_x) >> 8; ++block_x) {
            const auto* block = load_block(block_x, block_y,
                static_cast<std::uint8_t>(origin.z));
            if (block == nullptr) return std::nullopt;
            blocks.push_back(*block);
        }
    }
    return network::make_state6_path_search_grid(origin, blocks);
}

std::optional<AutomapView> AutomapStore::update(const DecodedMap& map,
    const resources::TibiaDatFile& dat, std::uint16_t player_x,
    std::uint16_t player_y, std::uint8_t player_floor,
    const AutomapViewState& state) {
    for (const auto& tile : map.tiles) {
        const auto ordered = resolve_native_tile_object_order(tile, dat);
        if (!ordered) continue;
        std::uint8_t color = 0;
        for (const auto* object : *ordered) {
            if (object->creature_id != 0 || object->type == 0 || object->type == 0x63)
                continue;
            const auto* item = dat.find_item(object->type);
            if (item == nullptr) continue;
            const auto* attribute = item->find_attribute(0x1d);
            if (attribute != nullptr && !attribute->payload_words.empty())
                color = static_cast<std::uint8_t>(attribute->payload_words.front());
        }
        const auto block_x = static_cast<std::uint32_t>(tile.x) >> 8;
        const auto block_y = static_cast<std::uint32_t>(tile.y) >> 8;
        auto* block = load_block(block_x, block_y, tile.floor);
        const auto offset = static_cast<std::size_t>(tile.y & 0xffU) * 256U +
            static_cast<std::size_t>(tile.x & 0xffU);
        std::uint8_t path_value = 0xfa;
        bool blocked = ordered->empty();
        for (const auto* object : *ordered) {
            if (object->type == 0x63 && object->creature_id != 0) {
                blocked = true;
                continue;
            }
            if (object->type == 0 || object->creature_id != 0) continue;
            const auto* item = dat.find_item(object->type);
            if (item == nullptr) continue;
            if (item->has_native_property(0x10) ||
                item->has_native_property(0x0d))
                blocked = true;
            if (!item->has_native_property(0x00)) continue;
            const auto* ground = item->find_attribute(0x00);
            if (ground == nullptr || ground->payload_words.empty()) continue;
            path_value = static_cast<std::uint8_t>(std::min<std::uint16_t>(
                path_value, ground->payload_words.front()));
        }
        if (blocked) path_value = 0xff;

        block->map_values[offset] = color;
        block->path_values[offset] = path_value;
        const auto found = blocks_.find(block_key(block_x, block_y, tile.floor));
        if (found != blocks_.end()) found->second.dirty = true;
    }

    if (state.floor >= 16 || state.zoom < 1) return std::nullopt;
    AutomapView view;
    view.center_x = state.center_x;
    view.center_y = state.center_y;
    view.floor = state.floor;
    view.zoom = state.zoom;
    auto sample_center_x = state.center_x;
    auto sample_center_y = state.center_y;
    if (state.zoom == 1) {
        sample_center_x = sample_center_x / 2 * 2;
        sample_center_y = sample_center_y / 2 * 2;
    }
    for (std::size_t y = 0; y < automap_view_size; ++y) {
        for (std::size_t x = 0; x < automap_view_size; ++x) {
            const auto world_x = (state.zoom * sample_center_x + state.zoom / 2 -
                0x6a + static_cast<std::int32_t>(x) * 2) / state.zoom;
            const auto world_y = (state.zoom * sample_center_y + state.zoom / 2 -
                0x6a + static_cast<std::int32_t>(y) * 2) / state.zoom;
            if (world_x < 0 || world_y < 0 || world_x > 0xffff || world_y > 0xffff)
                continue;
            auto* block = load_block(static_cast<std::uint32_t>(world_x) >> 8,
                static_cast<std::uint32_t>(world_y) >> 8, state.floor);
            const auto offset = static_cast<std::size_t>(world_y & 0xff) * 256U +
                static_cast<std::size_t>(world_x & 0xff);
            view.pixels[y * automap_view_size + x] = automap_color(block->map_values[offset]);
        }
    }
    if (view.floor == player_floor) {
        view.player_pixel_x = ((static_cast<std::int32_t>(player_x) - view.center_x) *
            state.zoom) / 2 + 0x35;
        view.player_pixel_y = ((static_cast<std::int32_t>(player_y) - view.center_y) *
            state.zoom) / 2 + 0x35;
        view.player_visible = view.player_pixel_x >= 0 && view.player_pixel_x < 0x69 &&
            view.player_pixel_y >= 0 && view.player_pixel_y < 0x69;
    }
    return view;
}

}
