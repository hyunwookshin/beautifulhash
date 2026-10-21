#pragma once

#include <cstdint>
#include <string>

#include "hash.hpp"
#include "scene.hpp"
#include "theme.hpp"

// Sky namespace (subspec/sky.md): one cloud and the sky's tint.
namespace bh::sky {

// Bits per field, in reading order.
inline constexpr int kTopBits = 3;     // cloud top edge height
inline constexpr int kHeightBits = 2;  // cloud height
inline constexpr int kWidthBits = 3;   // top edge width
inline constexpr int kCenterBits = 3;  // top edge center x
inline constexpr int kBottomBits = 2;  // bottom edge width, as a fraction of the top
inline constexpr int kSkewBits = 2;    // bottom edge shift
inline constexpr int kLeadBits = 1;    // leading (crisp) side: 0 left, 1 right
inline constexpr int kTintBits = 8;    // sky tint

struct Values {
    uint32_t top = 0, height = 0, width = 0, center = 0, bottom = 0, skew = 0, lead = 0, tint = 0;
};

Values decode(const Hash256& hash);
void encode(const Values& values, Hash256& hash);

// The 16 cloud bits as one number, in reading order (top first).
uint32_t cloud_bits(const Values& v);
Values with_cloud_bits(Values v, uint32_t bits);

// Builds the sky. The fog's density sets how clearly the cloud shows.
Sky build(const Values& v, const Fog& fog);
Sky generate(const Hash256& hash, const Fog& fog);

// The sky tint: 256 light blues, the sky gradient's top color.
uint32_t tint(uint32_t index);

// The second cloud (subspec/sky.md, Second cloud): 10 bits in Global (bits
// 6-15), its other fields derived from them and the first cloud's.
inline constexpr int kSecondBits = 10;
struct Second {
    uint32_t top = 0, width = 0, center = 0, lead = 0;  // 3, 3, 3, 1 bits
};
Second decode_second(const Hash256& hash);
void encode_second(const Second& w, Hash256& hash);
uint32_t second_bits(const Second& w);
Second with_second_bits(uint32_t bits);
Sky build_second(const Second& w, const Values& first, const Fog& fog);
Sky generate_second(const Hash256& hash, const Fog& fog);

// Whether the second cloud becomes an aurora: Tundra, at night, with the cloud
// wholly in the upper sky band (subspec/sky.md, Aurora).
bool aurora(Theme theme, const Sun& sun, const Sky& second);
// The aurora: its gradients and filters (in <defs>), and the curtain and
// ribbon through the second cloud's five points, lowest to highest.
std::string aurora_defs_svg(const Sky& second, double scale);
std::string aurora_svg(const Sky& second, double scale);
// The aurora's faint green cast on the ridges (and their snow), clipped to
// the ridges' paths.
std::string aurora_tint_svg(const Sky& second, int ridges, uint32_t width, uint32_t height);

// SVG pieces shared by render_svg and verify, at the given scale.
// The sky gradient element (in <defs>), warmed by a low sun.
std::string gradient_svg(const Sky& sky, const Sun& sun);
// The cloud: its glow gradients and clip path (in <defs>), the clip path's
// outline, and the clipped glows.
std::string cloud_defs_svg(const Sky& sky, const Sun& sun, double scale);
std::string cloud_path(const Sky& sky, double scale);
std::string cloud_svg(const Sky& sky, double scale);

}  // namespace bh::sky
