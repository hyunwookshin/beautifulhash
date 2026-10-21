// Renders one hash to SVG on stdout: render <64 hex digits> [width height].
// The native counterpart of the site's WebAssembly build.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "generate.hpp"
#include "hash.hpp"

int main(int argc, char** argv) {
    if (argc < 2 || std::string(argv[1]).size() != 64) {
        std::fprintf(stderr, "usage: render <64 hex digits> [width height]\n");
        return 2;
    }
    uint32_t w = argc > 3 ? static_cast<uint32_t>(std::atoi(argv[2])) : 1024;
    uint32_t h = argc > 3 ? static_cast<uint32_t>(std::atoi(argv[3])) : 768;
    std::fputs(bh::render_svg(bh::generate_scene(bh::Hash256::from_hex(argv[1]), 0), w, h).c_str(), stdout);
    return 0;
}
