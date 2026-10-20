#include "client/move_item_dialog_state.hpp"

namespace tibia::client {

std::optional<MoveItemDialogState> make_move_item_dialog_state(
    const network::ItemPosition& source,
    MoveItemDestination destination,
    std::uint8_t source_count) noexcept {
    if (source_count == 0 || source_count > 100) return std::nullopt;
    return MoveItemDialogState{source, destination, source_count, source_count};
}

bool set_move_item_dialog_amount(
    MoveItemDialogState& state,
    std::uint8_t amount) noexcept {
    if (amount == 0 || amount > state.maximum_amount ||
        state.maximum_amount > 100)
        return false;
    state.amount = amount;
    return true;
}

std::optional<MoveItemDialogRequest> resolve_move_item_dialog_request(
    const MoveItemDialogState& state) noexcept {
    if (state.maximum_amount == 0 || state.maximum_amount > 100 ||
        state.amount == 0 || state.amount > state.maximum_amount)
        return std::nullopt;
    return MoveItemDialogRequest{state.source, state.destination, state.amount};
}

std::optional<std::array<std::uint8_t, 15>> encode_move_item_dialog_request(
    const MoveItemDialogState& state) noexcept {
    const auto request = resolve_move_item_dialog_request(state);
    if (!request) return std::nullopt;
    return network::encode_move_item_request(request->source,
        request->destination.x, request->destination.y,
        request->destination.z, request->amount);
}

}
