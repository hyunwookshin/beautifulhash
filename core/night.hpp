#pragma once

#include "scene.hpp"
#include "theme.hpp"

// Night (subspec/global.md, Night): when the light is the moon's, every color in
// the scene is moved darker, grayer, and bluer, far layers less than near ones.
// The sky and cloud take their own night colors (palette.hpp).
namespace bh::night {

// Temperate ridges, wooded and unfaceted, go nearly to silhouettes.
void apply(Scene& scene, Theme theme, Season season);

}  // namespace bh::night
