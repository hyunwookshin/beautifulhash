#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace bh {

// A 256-bit input hash. Bit 0 is the most significant bit of byte 0;
// bit 255 is the least significant bit of byte 31.
struct Hash256 {
    std::array<uint8_t, 32> bytes{};

    static Hash256 from_hex(const std::string& hex);
    std::string to_hex() const;

    bool bit(int index) const;
    void set_bit(int index, bool value);
};

// Reads one namespace's slice of a hash, most significant bit first.
// Reading past the end of the slice aborts, so a namespace can never
// consume bits it does not own.
class BitReader {
public:
    BitReader(const Hash256& hash, int begin, int width);

    uint32_t read(int count);
    int position() const { return pos_; }  // namespace-relative
    int remaining() const { return width_ - pos_; }

private:
    const Hash256& hash_;
    int begin_;
    int width_;
    int pos_ = 0;
};

// The inverse of BitReader, used by tests to re-encode decoded values.
class BitWriter {
public:
    BitWriter(Hash256& hash, int begin, int width);

    void write(int count, uint32_t value);
    int position() const { return pos_; }

private:
    Hash256& hash_;
    int begin_;
    int width_;
    int pos_ = 0;
};

}  // namespace bh
