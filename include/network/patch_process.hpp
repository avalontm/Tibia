#pragma once

namespace tibia::network {

// Called after the main window loop has returned when a Tibia.exe update
// completed. On Windows, successful execution replaces this process with
// patch.exe; failure returns -1 and leaves errno set by the CRT.
[[nodiscard]] int execute_observed_external_patcher() noexcept;

} // namespace tibia::network
