#include "network/rsa_login_block.hpp"

#include <algorithm>
#include <stdexcept>

namespace tibia::network {
namespace {

class BlockWriter {
public:
    explicit BlockWriter(RsaLoginBlock& block) noexcept : block_(block) {}

    void write_u8(std::uint8_t value) {
        require_space(1);
        block_[position_++] = value;
    }

    void write_u32_le(std::uint32_t value) {
        require_space(4);
        for (unsigned i = 0; i < 4; ++i) {
            block_[position_++] = static_cast<std::uint8_t>(value >> (i * 8U));
        }
    }

    void write_bytes(std::span<const std::uint8_t> bytes) {
        require_space(bytes.size());
        std::copy(bytes.begin(), bytes.end(), block_.begin() + static_cast<std::ptrdiff_t>(position_));
        position_ += bytes.size();
    }

    void write_string(std::string_view value) {
        if (value.size() > 0xFFFFU) {
            throw std::invalid_argument("RSA login string exceeds the 16-bit length field");
        }
        require_space(2U + value.size());
        block_[position_++] = static_cast<std::uint8_t>(value.size());
        block_[position_++] = static_cast<std::uint8_t>(value.size() >> 8U);
        std::transform(value.begin(), value.end(), block_.begin() + static_cast<std::ptrdiff_t>(position_),
                       [](char byte) { return static_cast<std::uint8_t>(byte); });
        position_ += value.size();
    }

private:
    void require_space(std::size_t count) const {
        if (count > block_.size() - position_) {
            throw std::invalid_argument("RSA login fields exceed the 128-byte input block");
        }
    }

    RsaLoginBlock& block_;
    std::size_t position_{};
};

} // namespace

RsaLoginBlock make_login_rsa_block(
    const crypto::XteaKeyBytes& xtea_key,
    std::uint32_t account_number,
    std::string_view password,
    std::span<const std::uint8_t, 128> random_block) {
    RsaLoginBlock block{};
    std::copy(random_block.begin(), random_block.end(), block.begin());
    BlockWriter writer(block);
    writer.write_u8(0);
    writer.write_bytes(xtea_key);
    writer.write_u32_le(account_number);
    writer.write_string(password);
    return block;
}

RsaLoginBlock make_game_login_rsa_block(
    const crypto::XteaKeyBytes& xtea_key,
    bool flag,
    std::uint32_t account_number,
    std::string_view character_name,
    std::string_view password,
    std::span<const std::uint8_t, 128> random_block) {
    RsaLoginBlock block{};
    std::copy(random_block.begin(), random_block.end(), block.begin());
    BlockWriter writer(block);
    writer.write_u8(0);
    writer.write_bytes(xtea_key);
    writer.write_u8(flag ? 1 : 0);
    writer.write_u32_le(account_number);
    writer.write_string(character_name);
    writer.write_string(password);
    return block;
}

} // namespace tibia::network
