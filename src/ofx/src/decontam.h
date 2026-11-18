// cvt1.59: decontaminated edges. Where the matte is semi-transparent (hair, motion blur, soft
// edges) the source pixel is a mix of the subject and the background behind it: contaminated
// edge pixels. Premultiplied, they carry the old background into the new one.
//
// cvt1.60: strict edge extend (own code). The solid core (matte >= 0.98) is eroded by Alpha
// Erode; its colour is pushed outwards by a push-pull pyramid (multi-scale normalized
// convolution) and replaces every other subject pixel, so nothing of the old background (its
// colour, texture or lettering) is left in the edges. The matte is not changed, and the pure
// background and faint haze far from the subject keep the source. (cvt1.59 estimated each edge
// pixel's own colour with Blur-Fusion, which let background detail through.)
#pragma once

#include <vector>

namespace ltxag {

struct DecontamSettings {
  float erodeWidth = 1.0f;  // Alpha Erode (default 2 until cvt1.64): pixels the solid core is eroded by before its colour is extended
  float edgeFade = 2.0f;    // Edge Fade (default 4 until cvt1.64) (cvt1.61, then called Edge Blend): pixels over which the extended colour
                            // crossfades to the source, further in
  float edgeBlur = 0.0f;    // Edge Blur (cvt1.62): blur near the edge, about a Gaussian of this many pixels
  float scale = 1.0f;       // picture height / 1080: scales kDecontamReach
};

constexpr float kDecontamCore = 0.98f;    // matte at or above this is solid subject
constexpr float kDecontamBg = 0.02f;      // matte below this is background
constexpr float kDecontamReach = 256.0f;  // px at 1080p: matte further than this from the core is haze

// rgb: w*h*3 floats, alpha: w*h, both in the same (top-down or bottom-up) row order.
// out: w*h*3, the decontaminated picture (straight, not premultiplied). Values are in the
// colour space of rgb: the estimate holds in the space the comp is done in.
void decontaminate(const float* rgb, const float* alpha, int w, int h, const DecontamSettings& s,
                   std::vector<float>& out);

// Helpers, exposed for the tests.
// Fill val (ch channels) where weight is 0 from weighted neighbours at coarser scales.
void pushPull(const float* val, const float* weight, int ch, int w, int h, std::vector<float>& out);

}  // namespace ltxag
