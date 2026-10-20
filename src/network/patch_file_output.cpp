#include "network/patch_file_output.hpp"

#include "network/patch_transfer.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace tibia::network {

std::filesystem::path observed_patch_output_path(
    const std::filesystem::path& output_directory,
    State4PatchFile file) {
    const auto input_name = patch_file_name(file);
    if (input_name.empty()) {
        throw std::invalid_argument("patch output file code is not recognized");
    }

    std::string output_name;
    if (file == State4PatchFile::tibia_exe) {
        output_name = "Tibia.bak";
    } else {
        output_name.assign(input_name);
        output_name.push_back('_');
    }
    return output_directory / output_name;
}

std::vector<std::uint8_t> read_observed_patch_base_file(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open patch base file");
    }
    const auto end = input.tellg();
    if (end < 0) {
        throw std::runtime_error("could not determine patch base file size");
    }
    const auto size = static_cast<std::uint64_t>(end);
    std::vector<std::uint8_t> bytes;
    if (size > bytes.max_size() || size > static_cast<std::uint64_t>(
            std::numeric_limits<std::streamsize>::max())) {
        throw std::length_error("patch base file exceeds the supported size");
    }
    bytes.resize(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    if (!input) {
        throw std::runtime_error("could not seek to the start of the patch base file");
    }
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (input.gcount() != static_cast<std::streamsize>(bytes.size())) {
            throw std::runtime_error("could not read the complete patch base file");
        }
    }
    return bytes;
}

void write_observed_patch_output(
    const std::filesystem::path& output_directory,
    State4PatchFile file,
    std::span<const std::uint8_t> bytes) {
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::length_error("patch output exceeds the stream size limit");
    }

    const auto path = observed_patch_output_path(output_directory, file);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not open patch output file");
    }
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    output.close();
    if (!output) {
        throw std::runtime_error("could not write patch output file");
    }
}

void commit_observed_patch_output(
    const std::filesystem::path& output_directory,
    State4PatchFile file) {
    if (file == State4PatchFile::tibia_exe) {
        throw std::invalid_argument("Tibia.exe is committed by the external patcher path");
    }

    const auto staged_path = observed_patch_output_path(output_directory, file);
    const auto base_path = output_directory / patch_file_name(file);
    std::error_code error;
    if (!std::filesystem::exists(staged_path, error) || error) {
        throw std::runtime_error("staged patch output does not exist");
    }

    // The client attempts DeleteFileA first and then MoveFileA; it does not
    // make the replacement atomic or preserve the old base on move failure.
    error.clear();
    std::filesystem::remove(base_path, error);
    error.clear();
    std::filesystem::rename(staged_path, base_path, error);
    if (error) {
        throw std::runtime_error("could not move staged patch output into place");
    }
}

} // namespace tibia::network
