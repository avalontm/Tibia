#pragma once

#include "map/model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace tibia::map {

inline constexpr std::size_t floor_window_width = 18;
inline constexpr std::size_t floor_window_height = 14;
inline constexpr std::size_t floor_window_depth = 8;
inline constexpr std::size_t floor_window_size =
    floor_window_width * floor_window_height * floor_window_depth;

struct FloorWindowShift {
    std::int32_t steps{};
    std::vector<std::uint8_t> refreshed_layers;
};

class FloorWindow {
public:
    using ObjectStack = std::vector<Object>;

    struct Cell {
        ObjectStack objects;
        std::int32_t effect_head{-1};
        std::vector<std::int32_t> effect_chain;
        bool effect_chain_complete{true};
        bool dirty{};
    };

    struct CellClearEffect {
        std::vector<std::uint32_t> creature_ids;
        std::vector<std::int32_t> effect_ids;
        std::optional<std::int32_t> unresolved_effect_head;
    };

    [[nodiscard]] Cell& at(std::size_t x, std::size_t y, std::size_t floor);
    [[nodiscard]] const Cell& at(
        std::size_t x, std::size_t y, std::size_t floor) const;
    void reset() noexcept;
    void set_effect_head(
        std::size_t x, std::size_t y, std::size_t floor, std::int32_t effect_head);
    [[nodiscard]] bool link_effect(
        std::size_t x, std::size_t y, std::size_t floor, std::int32_t effect_id);
    [[nodiscard]] bool unlink_effect(
        std::size_t x, std::size_t y, std::size_t floor, std::int32_t effect_id);
    void mark_changed(std::size_t x, std::size_t y, std::size_t floor);
    [[nodiscard]] CellClearEffect clear_cell(
        std::size_t x, std::size_t y, std::size_t floor, bool release_effects);
    [[nodiscard]] FloorWindowShift shift(std::int32_t steps);

private:
    [[nodiscard]] static std::size_t index(
        std::size_t x, std::size_t y, std::size_t floor);
    std::array<Cell, floor_window_size> cells_{};
};

}
