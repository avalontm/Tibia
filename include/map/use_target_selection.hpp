#pragma once

#include "map/model.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace tibia::resources {
struct TibiaDatFile;
}

namespace tibia::map {

[[nodiscard]] std::optional<std::size_t> select_native_use_target_stack_position(
    const std::vector<const Object*>& objects, const resources::TibiaDatFile& dat);

}
