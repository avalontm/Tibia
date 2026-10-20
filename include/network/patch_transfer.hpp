#pragma once

#include "network/message_dispatcher.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

namespace tibia::network {

struct CompletedPatchFile {
    State4PatchFile file{};
    std::vector<std::uint8_t> bytes;
    PatchPayloadMode mode{PatchPayloadMode::full_file};
};

[[nodiscard]] std::string_view patch_file_name(State4PatchFile file) noexcept;

enum class PatchCompletionActionKind : std::uint8_t {
    commit_staged_output,
    signal_patch_processed,
    reload_tibia_dat,
    reload_tibia_spr,
    reload_tibia_pic,
    transition_to_previous_state,
    launch_patch_exe_after_client_shutdown,
};

struct PatchCompletionAction {
    PatchCompletionActionKind kind{};
    // The original internal event/message number when the action has one.
    std::uint8_t observed_event{};
};

// Returns the client's ordered post-finalization actions for one file type.
[[nodiscard]] std::vector<PatchCompletionAction> observed_patch_completion_actions(
    State4PatchFile file);

struct PatchTransferProgress {
    std::uint32_t received{};
    std::uint32_t total{};
    std::uint8_t percent{};
    bool complete{};
};

// Reconstructs the observed 0x1E/0x1F/0x20 in-memory patch transfer. It does
// not write or replace files; callers decide what to do with a completed payload.
class PatchTransfer {
public:
    void begin(std::uint8_t file_code, std::uint32_t total_size, PatchPayloadMode mode);
    [[nodiscard]] PatchTransferProgress append(std::span<const std::uint8_t> chunk);
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] CompletedPatchFile take_completed();

private:
    std::uint8_t file_code_{};
    std::uint32_t total_size_{};
    PatchPayloadMode mode_{PatchPayloadMode::full_file};
    std::vector<std::uint8_t> bytes_;
    bool active_{};
};

struct PatchTransferCallbacks {
    std::function<void(const PatchTransferProgress&)> on_progress;
    std::function<void(CompletedPatchFile)> on_completed;
};

// Connects state 4 opcodes 0x1E and 0x1F to one in-memory transfer instance.
// A completed payload is moved to on_completed; no filesystem operation occurs.
void register_observed_patch_transfer_handlers(
    MessageDispatcher& dispatcher,
    PatchTransfer& transfer,
    PatchTransferCallbacks callbacks);

} // namespace tibia::network
