// Renders comparison galleries: each parameter sweep (tools/variants.hpp) is
// rendered on the same random hashes, in two- and three-ridge mode. Writes SVGs
// plus an index.html.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "celestial.hpp"
#include "fog.hpp"
#include "generate.hpp"
#include "hills.hpp"
#include "trees.hpp"
#include "mountains.hpp"
#include "partition.hpp"
#include "variants.hpp"
#include "vegetation.hpp"

using namespace bh;
namespace mt = bh::mountains;

namespace {

void write_file(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary);
    f << text;
}

// Gallery only: rewrites the Trees count code so a column shows a fixed count.
void force_tree_count(Hash256& h, int n) {
    BitWriter w(h, kTrees.begin, kTrees.width);
    if (n >= 2 && n <= 7) {
        w.write(3, static_cast<uint32_t>(n - 2));
    } else {
        int idx = n == 1 ? 0 : n - 7;
        w.write(3, static_cast<uint32_t>(6 + idx / 2));
        w.write(1, static_cast<uint32_t>(idx % 2));
    }
}

std::string tree_count_label(int n) { return std::to_string(n) + (n == 1 ? " tree" : " trees"); }

}  // namespace

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "gallery";
    int per_mode = argc > 2 ? std::atoi(argv[2]) : 6;
    uint64_t seed = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 2026;
    mkdir(dir.c_str(), 0755);

    // The same hashes are used for every variant.
    std::mt19937_64 rng(seed);
    std::vector<Hash256> hashes;
    for (int mode : {2, 3}) {
        for (int i = 0; i < per_mode; ++i) {
            Hash256 h;
            for (auto& b : h.bytes) b = static_cast<uint8_t>(rng());
            h.set_bit(kMountains.begin, mode == 3);  // header: 0 = two ridges, 1 = three
            hashes.push_back(h);
        }
    }

    std::vector<Variant> all = variants();
    std::vector<mt::Config> configs;
    for (const Variant& v : all) configs.push_back(mt::make_config(v.params));

    std::string html =
        "<!doctype html><meta charset=\"utf-8\"><title>BeautifulHash Gallery</title>\n"
        "<style>body{font:14px system-ui,sans-serif;margin:24px;background:#f4f4f2;color:#222}"
        "h2{margin-top:40px}table{border-collapse:separate;border-spacing:8px}"
        "th{font-weight:600;text-align:center}td.h{font:11px monospace;color:#888;max-width:90px;word-break:break-all}"
        "img{width:400px;display:block;border-radius:4px;box-shadow:0 1px 3px #0003}"
        "</style>\n"
        "<h1>BeautifulHash Gallery</h1>\n"
        "<p>Seed " + std::to_string(seed) + ". One table per biome, one column per season. Every cell in a "
        "row uses the same hash, so only the biome and season change. Mountains, hills, trees, vegetation, and fog all "
        "come from the hash.</p>\n";

    for (const Group& g : variant_groups()) {
        html += "<h2>" + g.description + "</h2><div style=\"overflow-x:auto\"><table><tr><th></th>";
        std::vector<size_t> members;
        for (size_t v = 0; v < all.size(); ++v) {
            if (all[v].group == g.name) {
                members.push_back(v);
                html += "<th>" + all[v].label + "</th>";
            }
        }
        html += "</tr>\n";
        for (size_t i = 0; i < hashes.size(); ++i) {
            html += "<tr><td class=\"h\">" + std::string(i < static_cast<size_t>(per_mode) ? "2 ridges" : "3 ridges") +
                    "<br>" + tree_count_label(trees::decode(hashes[i], false, Theme::kAlpine, Season::kSummer).count) +
                    "<br>" + hashes[i].to_hex().substr(12, 22) + "&hellip;</td>";
            for (size_t v : members) {
                Scene scene;
                scene.ridges = mt::generate(configs[v], hashes[i]);
                scene.hills = hills::generate(hashes[i], configs[v].params.theme, configs[v].params.season);
                Hash256 th = hashes[i];
                if (all[v].trees > 0) force_tree_count(th, all[v].trees);
                scene.trees = trees::generate(th, scene.hills, configs[v].params.theme, configs[v].params.season);
                trees::add_crest_trees(scene.hills, hashes[i], configs[v].params.theme, configs[v].params.season);
                scene.sun = celestial::generate(hashes[i]);
                hills::add_bands(scene.hills[0], scene.sun);
                celestial::cast_shadows(scene.trees, scene.hills[0], scene.sun);
                trees::tint_cover(scene.trees, scene.hills[0]);
                scene.vegetation = vegetation::generate(hashes[i], scene.trees, scene.hills, configs[v].params.theme, configs[v].params.season);
                scene.fog = fog::generate(hashes[i], configs[v].params.theme, configs[v].params.season);
                std::string base = "v" + std::to_string(v) + "_h" + std::to_string(i) + ".svg";
                write_file(dir + "/" + base, render_svg(scene, 1024, 768));
                html += "<td><img src=\"" + base + "\"></td>";
            }
            html += "</tr>\n";
        }
        html += "</table></div>\n";
    }
    write_file(dir + "/index.html", html);
    std::printf("wrote %zu variants x %zu hashes to %s/index.html\n", all.size(), hashes.size(), dir.c_str());
    return 0;
}
