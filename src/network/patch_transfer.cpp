#include "network/patch_transfer.hpp"

#include <stdexcept>
#include <utility>

namespace tibia::network {

std::string_view patch_file_name(State4PatchFile file) noexcept {
    switch (file) {
    case State4PatchFile::tibia_exe: return "Tibia.exe";
    case State4PatchFile::tibia_dat: return "Tibia.dat";
    case State4PatchFile::tibia_spr: return "Tibia.spr";
    case State4PatchFile::tibia_pic: return "Tibia.pic";
    case State4PatchFile::patch_exe: return "Patch.exe";
    }
    return {};
}

std::vector<PatchCompletionAction> observed_patch_completion_actions(
    State4PatchFile file) {
    using Action = PatchCompletionActionKind;
    switch (file) {
    case State4PatchFile::tibia_exe:
        return {{Action::signal_patch_processed, 0x43},
                {Action::launch_patch_exe_after_client_shutdown, 0x4C}};
    case State4PatchFile::tibia_dat:
        return {{Action::commit_staged_output, 0x44},
                {Action::signal_patch_processed, 0x43},
                {Action::reload_tibia_dat, 0x49},
                {Action::transition_to_previous_state, 0x42}};
    case State4PatchFile::tibia_spr:
        return {{Action::commit_staged_output, 0x45},
                {Action::signal_patch_processed, 0x43},
                {Action::reload_tibia_spr, 0x4A},
                {Action::transition_to_previous_state, 0x42}};
    case State4PatchFile::tibia_pic:
        return {{Action::commit_staged_output, 0x46},
                {Action::signal_patch_processed, 0x43},
                {Action::reload_tibia_pic, 0x4B},
                {Action::transition_to_previous_state, 0x42}};
    case State4PatchFile::patch_exe:
        return {{Action::commit_staged_output, 0x47},
                {Action::signal_patch_processed, 0x43}};
    }
    throw std::invalid_argument("patch completion file code is not recognized");
}

void PatchTransfer::begin(
    std::uint8_t file_code,
    std::uint32_t total_size,
    PatchPayloadMode mode) {
    if (active_) {
        throw std::logic_error("a patch transfer is already active");
    }
    if (file_code > 4 || (mode == PatchPayloadMode::delta_stream && file_code == 4)) {
        throw std::invalid_argument("patch transfer file code is not recognized");
    }
    if (mode != PatchPayloadMode::full_file && mode != PatchPayloadMode::delta_stream) {
        throw std::invalid_argument("patch transfer payload mode is not recognized");
    }
    if (total_size == 0) {
        throw std::invalid_argument("patch transfer total size must be nonzero");
    }
    if (static_cast<std::uint64_t>(total_size) > bytes_.max_size()) {
        throw std::length_error("patch transfer exceeds the container limit");
    }

    file_code_ = file_code;
    total_size_ = total_size;
    mode_ = mode;
    bytes_.clear();
    bytes_.reserve(total_size);
    active_ = true;
}

PatchTransferProgress PatchTransfer::append(std::span<const std::uint8_t> chunk) {
    if (!active_) {
        throw std::logic_error("no patch transfer is active");
    }
    if (bytes_.size() > total_size_ || chunk.size() > total_size_ - bytes_.size()) {
        throw std::invalid_argument("patch chunk exceeds the announced total size");
    }

    bytes_.insert(bytes_.end(), chunk.begin(), chunk.end());
    const auto received = static_cast<std::uint32_t>(bytes_.size());
    const auto percent = static_cast<std::uint8_t>(
        (static_cast<std::uint64_t>(received) * 100U) / total_size_);
    return {received, total_size_, percent, received == total_size_};
}

bool PatchTransfer::active() const noexcept {
    return active_;
}

CompletedPatchFile PatchTransfer::take_completed() {
    if (!active_ || bytes_.size() != total_size_) {
        throw std::logic_error("patch transfer is not complete");
    }

    CompletedPatchFile completed{
        static_cast<State4PatchFile>(file_code_),
        std::move(bytes_),
        mode_,
    };
    file_code_ = 0;
    total_size_ = 0;
    mode_ = PatchPayloadMode::full_file;
    bytes_.clear();
    active_ = false;
    return completed;
}

void register_observed_patch_transfer_handlers(
    MessageDispatcher& dispatcher,
    PatchTransfer& transfer,
    PatchTransferCallbacks callbacks) {
    if (!callbacks.on_completed) {
        throw std::invalid_argument("completed patch consumer must be callable");
    }

    register_observed_state4_patch_update_handler(
        dispatcher,
        [&transfer](const State4PatchUpdate& update) {
            transfer.begin(update.file_code, update.total_size, update.mode);
        });

    register_observed_state4_patch_chunk_handler(
        dispatcher,
        [&transfer,
         on_progress = std::move(callbacks.on_progress),
         on_completed = std::move(callbacks.on_completed)](const State4PatchChunk& chunk) {
            const auto progress = transfer.append(chunk.bytes);
            if (on_progress) {
                on_progress(progress);
            }
            if (progress.complete) {
                on_completed(transfer.take_completed());
            }
        });
}

} // namespace tibia::network
