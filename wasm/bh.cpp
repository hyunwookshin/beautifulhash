// WebAssembly entry point: renders a 32-byte hash (64 hex digits) to an SVG,
// the same bytes as the native build (README, Native and WebAssembly).
#include <emscripten/emscripten.h>

#include <string>

#include "generate.hpp"
#include "hash.hpp"

extern "C" {

// Returns the SVG for `hex` at width x height, or "" for a malformed hash.
// The string stays valid until the next call.
EMSCRIPTEN_KEEPALIVE const char* bh_render(const char* hex, int width, int height) {
    static std::string out;
    std::string h(hex);
    out.clear();
    if (h.size() != 64 || h.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return out.c_str();
    out = bh::render_svg(bh::generate_scene(bh::Hash256::from_hex(h), 0), static_cast<uint32_t>(width),
                         static_cast<uint32_t>(height));
    return out.c_str();
}
}
