#include "network/patch_process.hpp"

#include <cerrno>

#ifdef _WIN32
#include <process.h>
#endif

namespace tibia::network {

int execute_observed_external_patcher() noexcept {
#ifdef _WIN32
    return _execl("patch.exe", "patch.exe", static_cast<const char*>(nullptr));
#else
    errno = ENOTSUP;
    return -1;
#endif
}

} // namespace tibia::network
