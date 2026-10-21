#include "global.hpp"

#include "partition.hpp"

namespace bh::global {

Values decode(const Hash256& hash) {
    BitReader in(hash, kGlobal.begin, 6);
    Values v;
    v.daylight = in.read(kDaylightBits);
    v.theme = in.read(2);
    v.season = in.read(2);
    return v;
}

void encode(const Values& values, Hash256& hash) {
    BitWriter out(hash, kGlobal.begin, 6);
    out.write(kDaylightBits, values.daylight);
    out.write(2, values.theme);
    out.write(2, values.season);
}

Theme theme(const Values& v) { return kThemes[v.theme]; }
Season season(const Values& v) { return kSeasons[v.season]; }

}  // namespace bh::global
