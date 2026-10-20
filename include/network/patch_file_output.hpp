#pragma once

#include "network/message_dispatcher.hpp"

#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace tibia::network {

[[nodiscard]] std::filesystem::path observed_patch_output_path(
    const std::filesystem::path& output_directory,
    State4PatchFile file);

[[nodiscard]] std::vector<std::uint8_t> read_observed_patch_base_file(
    const std::filesystem::path& path);

// Reproduces fopen(path, "wb") semantics at a caller-selected path.
void write_observed_patch_output(
    const std::filesystem::path& output_directory,
    State4PatchFile file,
    std::span<const std::uint8_t> bytes);

// Promotes a staged Tibia.dat/spr/pic/Patch.exe output into its base filename,
// matching the client's delete-then-MoveFile update event. Tibia.exe uses the
// separate Tibia.bak/external-patcher path and is not accepted here.
void commit_observed_patch_output(
    const std::filesystem::path& output_directory,
    State4PatchFile file);

} // namespace tibia::network
