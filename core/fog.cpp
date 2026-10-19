#include "fog.hpp"

#include <cstdio>
#include <cstdlib>

#include "palette.hpp"
#include "partition.hpp"

namespace bh::fog {

Values decode(const Hash256& hash) {
    BitReader in(hash, kFog.begin, kFog.width);
    Values v;
    v.density = in.read(kDensityBits);
    v.tint = in.read(kTintBits);
    v.mist = in.read(kMistBits);
    if (in.remaining() != 0) {
        std::fprintf(stderr, "fog: layout leaves %d bits unread\n", in.remaining());
        std::abort();
    }
    return v;
}

void encode(const Values& values, Hash256& hash) {
    BitWriter out(hash, kFog.begin, kFog.width);
    out.write(kDensityBits, values.density);
    out.write(kTintBits, values.tint);
    out.write(kMistBits, values.mist);
}

Fog build(const Values& values, Theme theme, Season season) {
    // Opacities in hundredths, so they serialize exactly. The density sets the
    // mean (27-36%); the mist tilts it toward the ground without changing it.
    int mean = 27 + 3 * static_cast<int>(values.density);
    int tilt = static_cast<int>(values.mist);
    Fog f;
    f.rgb = fog_palette(theme, season)[values.tint];
    f.top = mean - tilt;
    f.bottom = mean + tilt;
    return f;
}

Fog generate(const Hash256& hash, Theme theme, Season season) { return build(decode(hash), theme, season); }

}  // namespace bh::fog
