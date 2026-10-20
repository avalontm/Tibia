#pragma once

#include "network/rsa_login_block.hpp"

namespace tibia::network {

// Applies the fixed 1024-bit public RSA operation observed in Tibia.exe.
// The input and output are 128-byte big-endian integers; no PKCS#1 padding is
// added because the client constructs its own randomized RSA input block.
[[nodiscard]] RsaLoginBlock encrypt_login_block_rsa(const RsaLoginBlock& plaintext);

} // namespace tibia::network
