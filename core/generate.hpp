#pragma once

#include <cstdint>
#include <string>

#include "hash.hpp"
#include "scene.hpp"

namespace bh {

// Independent of resolution (SPECIFICATIONS.md, V0 compatibility guarantee).
// Only the Mountains namespace is implemented; other namespaces are ignored.
Scene generate_scene(const Hash256& hash, int version);

// Identical arguments give identical bytes on every supported platform.
std::string render_svg(const Scene& scene, uint32_t width, uint32_t height);

}  // namespace bh
