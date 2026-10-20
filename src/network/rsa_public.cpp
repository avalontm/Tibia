#include "network/rsa_public.hpp"

#ifndef _WIN32
#error "Tibia's reconstructed RSA path currently uses Windows CNG."
#endif

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace tibia::network {
namespace {

constexpr char public_modulus_decimal[] =
    "109120132967399429278860960508995541528237502902798129123468757937266291492576446330739696001110603907230888610072655818825358503429057592827629436413108566029093628212635953836686562675849720620786279431090218017681061521755056710823876476444260558147179707119674283982419152118103759076030616683978566631413";

constexpr ULONG rsa_bytes = 128;
constexpr ULONG public_exponent = 65537;

void check_status(NTSTATUS status, const char* operation) {
    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error(std::string(operation) + " failed (NTSTATUS " +
                                 std::to_string(static_cast<unsigned long>(status)) + ")");
    }
}

std::array<UCHAR, rsa_bytes> modulus_bytes() {
    std::array<UCHAR, rsa_bytes> modulus{};
    for (const char character : public_modulus_decimal) {
        if (character == '\0') {
            break;
        }
        if (character < '0' || character > '9') {
            throw std::logic_error("embedded RSA modulus is not decimal");
        }

        unsigned carry = static_cast<unsigned>(character - '0');
        for (auto it = modulus.rbegin(); it != modulus.rend(); ++it) {
            const unsigned value = static_cast<unsigned>(*it) * 10U + carry;
            *it = static_cast<UCHAR>(value & 0xFFU);
            carry = value >> 8U;
        }
        if (carry != 0) {
            throw std::logic_error("embedded RSA modulus exceeds 1024 bits");
        }
    }
    if ((modulus.front() & 0x80U) == 0) {
        throw std::logic_error("embedded RSA modulus is not 1024 bits");
    }
    return modulus;
}

struct CngHandles {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_KEY_HANDLE key{};

    ~CngHandles() {
        if (key) {
            BCryptDestroyKey(key);
        }
        if (algorithm) {
            BCryptCloseAlgorithmProvider(algorithm, 0);
        }
    }
};

} // namespace

RsaLoginBlock encrypt_login_block_rsa(const RsaLoginBlock& plaintext) {
    CngHandles handles;
    check_status(BCryptOpenAlgorithmProvider(&handles.algorithm, BCRYPT_RSA_ALGORITHM, nullptr, 0),
                 "BCryptOpenAlgorithmProvider(RSA)");

    const auto modulus = modulus_bytes();
    const std::array<UCHAR, 3> exponent{
        static_cast<UCHAR>((public_exponent >> 16U) & 0xFFU),
        static_cast<UCHAR>((public_exponent >> 8U) & 0xFFU),
        static_cast<UCHAR>(public_exponent & 0xFFU),
    };

    std::vector<UCHAR> public_blob(sizeof(BCRYPT_RSAKEY_BLOB) + exponent.size() + modulus.size());
    auto* header = reinterpret_cast<BCRYPT_RSAKEY_BLOB*>(public_blob.data());
    *header = BCRYPT_RSAKEY_BLOB{
        BCRYPT_RSAPUBLIC_MAGIC,
        1024,
        static_cast<ULONG>(exponent.size()),
        static_cast<ULONG>(modulus.size()),
        0,
        0,
    };
    auto* blob_data = public_blob.data() + sizeof(BCRYPT_RSAKEY_BLOB);
    std::memcpy(blob_data, exponent.data(), exponent.size());
    std::memcpy(blob_data + exponent.size(), modulus.data(), modulus.size());

    check_status(BCryptImportKeyPair(handles.algorithm, nullptr, BCRYPT_RSAPUBLIC_BLOB,
                                     &handles.key, public_blob.data(),
                                     static_cast<ULONG>(public_blob.size()), 0),
                 "BCryptImportKeyPair(RSA public key)");

    std::array<UCHAR, rsa_bytes> input{};
    RsaLoginBlock ciphertext{};
    std::copy(plaintext.begin(), plaintext.end(), input.begin());
    ULONG result_size = 0;
    check_status(BCryptEncrypt(handles.key, input.data(), static_cast<ULONG>(input.size()),
                               nullptr, nullptr, 0, ciphertext.data(),
                               static_cast<ULONG>(ciphertext.size()), &result_size,
                               BCRYPT_PAD_NONE),
                 "BCryptEncrypt(RSA without provider padding)");
    if (result_size != ciphertext.size()) {
        throw std::runtime_error("RSA provider returned a non-128-byte result");
    }
    return ciphertext;
}

} // namespace tibia::network
