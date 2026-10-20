#include "network/login_session.hpp"

#include "network/rsa_login_block.hpp"
#include "network/rsa_public.hpp"

#include <utility>

namespace tibia::network {
namespace {

PreparedLogin make_prepared_login(
    const crypto::XteaKeyBytes& key_bytes,
    std::vector<std::uint8_t> request) {
    return PreparedLogin{key_bytes, crypto::key_from_bytes(key_bytes), std::move(request)};
}

} // namespace

PreparedLogin prepare_login_server_session(
    const crypto::XteaKeyBytes& xtea_key,
    std::span<const std::uint8_t, 128> rsa_random_block,
    const LoginServerRequestFields& fields,
    std::uint32_t account_number,
    std::string_view password) {
    const auto plaintext = make_login_rsa_block(xtea_key, account_number, password, rsa_random_block);
    const auto ciphertext = encrypt_login_block_rsa(plaintext);
    return make_prepared_login(xtea_key, make_login_server_request(fields, ciphertext));
}

PreparedLogin prepare_game_server_session(
    const crypto::XteaKeyBytes& xtea_key,
    std::span<const std::uint8_t, 128> rsa_random_block,
    bool rsa_flag,
    std::uint32_t account_number,
    std::string_view character_name,
    std::string_view password) {
    const auto plaintext = make_game_login_rsa_block(
        xtea_key, rsa_flag, account_number, character_name, password, rsa_random_block);
    const auto ciphertext = encrypt_login_block_rsa(plaintext);
    return make_prepared_login(xtea_key, make_game_server_request(ciphertext));
}

} // namespace tibia::network
