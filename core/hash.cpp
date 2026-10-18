#include "hash.hpp"

#include <cstdio>
#include <cstdlib>

namespace bh {

namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    std::fprintf(stderr, "invalid hex digit '%c'\n", c);
    std::abort();
}

}  // namespace

Hash256 Hash256::from_hex(const std::string& hex) {
    if (hex.size() != 64) {
        std::fprintf(stderr, "hash must be 64 hex digits\n");
        std::abort();
    }
    Hash256 h;
    for (int i = 0; i < 32; ++i) {
        h.bytes[i] = static_cast<uint8_t>(hex_value(hex[2 * i]) << 4 | hex_value(hex[2 * i + 1]));
    }
    return h;
}

std::string Hash256::to_hex() const {
    static const char* digits = "0123456789abcdef";
    std::string s;
    s.reserve(64);
    for (uint8_t b : bytes) {
        s.push_back(digits[b >> 4]);
        s.push_back(digits[b & 15]);
    }
    return s;
}

bool Hash256::bit(int index) const {
    return (bytes[index / 8] >> (7 - index % 8)) & 1;
}

void Hash256::set_bit(int index, bool value) {
    uint8_t mask = static_cast<uint8_t>(1u << (7 - index % 8));
    if (value) {
        bytes[index / 8] |= mask;
    } else {
        bytes[index / 8] &= static_cast<uint8_t>(~mask);
    }
}

BitReader::BitReader(const Hash256& hash, int begin, int width)
    : hash_(hash), begin_(begin), width_(width) {}

uint32_t BitReader::read(int count) {
    if (count < 0 || count > 32 || pos_ + count > width_) {
        std::fprintf(stderr, "BitReader: read of %d bits at %d exceeds slice width %d\n",
                     count, pos_, width_);
        std::abort();
    }
    uint32_t value = 0;
    for (int i = 0; i < count; ++i) {
        value = (value << 1) | (hash_.bit(begin_ + pos_ + i) ? 1u : 0u);
    }
    pos_ += count;
    return value;
}

BitWriter::BitWriter(Hash256& hash, int begin, int width)
    : hash_(hash), begin_(begin), width_(width) {}

void BitWriter::write(int count, uint32_t value) {
    if (count < 0 || count > 32 || pos_ + count > width_) {
        std::fprintf(stderr, "BitWriter: write of %d bits at %d exceeds slice width %d\n",
                     count, pos_, width_);
        std::abort();
    }
    for (int i = 0; i < count; ++i) {
        hash_.set_bit(begin_ + pos_ + i, (value >> (count - 1 - i)) & 1);
    }
    pos_ += count;
}

}  // namespace bh
