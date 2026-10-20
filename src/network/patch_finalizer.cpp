#include "network/patch_finalizer.hpp"

#include "network/delta_patch.hpp"
#include "network/patch_file_output.hpp"

#include <stdexcept>
#include <vector>

namespace tibia::network {

std::filesystem::path finalize_observed_patch_file(
    const std::filesystem::path& output_directory,
    const CompletedPatchFile& patch,
    std::optional<std::uint32_t> expected_base_prefix) {
    const auto input_name = patch_file_name(patch.file);
    if (input_name.empty()) {
        throw std::invalid_argument("patch output file code is not recognized");
    }

    std::vector<std::uint8_t> output_bytes;
    switch (patch.mode) {
    case PatchPayloadMode::full_file:
        output_bytes = patch.bytes;
        break;
    case PatchPayloadMode::delta_stream: {
        const auto base = read_observed_patch_base_file(output_directory / input_name);
        auto base_prefix = expected_base_prefix;
        if (!base_prefix && patch.file != State4PatchFile::tibia_exe &&
            patch.file != State4PatchFile::patch_exe) {
            if (base.size() < sizeof(std::uint32_t)) {
                throw std::runtime_error("patch base is too small to contain its observed prefix");
            }
            base_prefix = static_cast<std::uint32_t>(base[0]) |
                (static_cast<std::uint32_t>(base[1]) << 8U) |
                (static_cast<std::uint32_t>(base[2]) << 16U) |
                (static_cast<std::uint32_t>(base[3]) << 24U);
        }
        output_bytes = apply_observed_delta_patch(base, patch.bytes, base_prefix);
        break;
    }
    default:
        throw std::invalid_argument("patch payload mode is not recognized");
    }

    const auto output_path = observed_patch_output_path(output_directory, patch.file);
    write_observed_patch_output(output_directory, patch.file, output_bytes);
    return output_path;
}

} // namespace tibia::network
