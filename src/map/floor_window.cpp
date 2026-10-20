#include "map/floor_window.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace tibia::map {

std::size_t FloorWindow::index(
    std::size_t x, std::size_t y, std::size_t floor) {
    if (x >= floor_window_width || y >= floor_window_height ||
        floor >= floor_window_depth)
        throw std::out_of_range("map floor-window coordinate is outside its dimensions");
    return x + (y + floor * floor_window_height) * floor_window_width;
}

FloorWindow::Cell& FloorWindow::at(
    std::size_t x, std::size_t y, std::size_t floor) {
    return cells_[index(x, y, floor)];
}

const FloorWindow::Cell& FloorWindow::at(
    std::size_t x, std::size_t y, std::size_t floor) const {
    return cells_[index(x, y, floor)];
}

void FloorWindow::reset() noexcept {
    for (auto& cell : cells_) cell = {};
}

void FloorWindow::set_effect_head(
    std::size_t x, std::size_t y, std::size_t floor, std::int32_t effect_head) {
    if (effect_head < -1 || effect_head >= 200)
        throw std::out_of_range("map effect number is outside the observed -1..199 range");
    auto& cell = at(x, y, floor);
    cell.effect_head = effect_head;
    cell.effect_chain.clear();
    cell.effect_chain_complete = effect_head == -1;
}

bool FloorWindow::link_effect(
    std::size_t x, std::size_t y, std::size_t floor, std::int32_t effect_id) {
    if (effect_id < 0 || effect_id >= 200)
        throw std::out_of_range("map effect number is outside the observed 0..199 range");
    auto& cell = at(x, y, floor);
    if (!cell.effect_chain_complete)
        throw std::logic_error("map effect chain is incomplete");
    if (std::find(cell.effect_chain.begin(), cell.effect_chain.end(), effect_id) !=
        cell.effect_chain.end()) return false;
    cell.effect_chain.insert(cell.effect_chain.begin(), effect_id);
    cell.effect_head = cell.effect_chain.front();
    return true;
}

bool FloorWindow::unlink_effect(
    std::size_t x, std::size_t y, std::size_t floor, std::int32_t effect_id) {
    auto& cell = at(x, y, floor);
    if (!cell.effect_chain_complete)
        throw std::logic_error("map effect chain is incomplete");
    const auto found = std::find(cell.effect_chain.begin(), cell.effect_chain.end(), effect_id);
    if (found == cell.effect_chain.end()) return false;
    cell.effect_chain.erase(found);
    cell.effect_head = cell.effect_chain.empty() ? -1 : cell.effect_chain.front();
    return true;
}

void FloorWindow::mark_changed(std::size_t x, std::size_t y, std::size_t floor) {
    at(x, y, floor).dirty = true;
}

FloorWindow::CellClearEffect FloorWindow::clear_cell(
    std::size_t x, std::size_t y, std::size_t floor, bool release_effects) {
    auto& cell = at(x, y, floor);
    CellClearEffect effect;
    for (const auto& object : cell.objects)
        if (object.creature_id != 0) effect.creature_ids.push_back(object.creature_id);
    cell.objects.clear();
    cell.dirty = false;
    if (release_effects) {
        if (cell.effect_chain_complete) {
            effect.effect_ids = std::move(cell.effect_chain);
            cell.effect_chain.clear();
            cell.effect_head = -1;
        } else {
            effect.unresolved_effect_head = cell.effect_head;
        }
    }
    return effect;
}

FloorWindowShift FloorWindow::shift(std::int32_t steps) {
    FloorWindowShift effect{steps, {}};
    auto remaining = steps;
    while (remaining > 0) {
        for (std::size_t x = 0; x < floor_window_width; ++x) {
            for (std::size_t y = 0; y < floor_window_height; ++y) {
                auto last = std::move(at(x, y, floor_window_depth - 1));
                for (std::size_t floor = floor_window_depth - 1; floor > 0; --floor)
                    at(x, y, floor) = std::move(at(x, y, floor - 1));
                at(x, y, 0) = std::move(last);
            }
        }
        effect.refreshed_layers.push_back(0);
        --remaining;
    }
    while (remaining < 0) {
        for (std::size_t x = 0; x < floor_window_width; ++x) {
            for (std::size_t y = 0; y < floor_window_height; ++y) {
                auto first = std::move(at(x, y, 0));
                for (std::size_t floor = 0; floor + 1 < floor_window_depth; ++floor)
                    at(x, y, floor) = std::move(at(x, y, floor + 1));
                at(x, y, floor_window_depth - 1) = std::move(first);
            }
        }
        effect.refreshed_layers.push_back(static_cast<std::uint8_t>(floor_window_depth - 1));
        ++remaining;
    }
    if (steps > 0) {
        effect.refreshed_layers.insert(effect.refreshed_layers.end(), {5, 6, 7});
    }
    return effect;
}

}
