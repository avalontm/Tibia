#pragma once

#include "network/patch_transfer.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace tibia::network {

// Applies one completed transfer using the observed finalizer mode and output
// naming. Delta mode reads its base file from output_directory. When no base
// prefix is supplied, data/sprite/picture deltas use the first little-endian
// word of that base, matching the client's cached resource prefix.
[[nodiscard]] std::filesystem::path finalize_observed_patch_file(
    const std::filesystem::path& output_directory,
    const CompletedPatchFile& patch,
    std::optional<std::uint32_t> expected_base_prefix = std::nullopt);

} // namespace tibia::network
