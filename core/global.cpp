#include "global.hpp"

#include "partition.hpp"

namespace bh::global {

Values decode(const Hash256& hash) {
    BitReader in(hash, kGlobal.begin, kDaylightBits);
    Values v;
    v.daylight = in.read(kDaylightBits);
    return v;
}

void encode(const Values& values, Hash256& hash) {
    BitWriter out(hash, kGlobal.begin, kDaylightBits);
    out.write(kDaylightBits, values.daylight);
}

}  // namespace bh::global
