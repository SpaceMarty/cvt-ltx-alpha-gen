// Plain-assert tests for decontam (cvt1.59, strict edge extend since cvt1.60); run via ctest.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

#include "decontam.h"

using namespace ltxag;

static int failures = 0;
#define CHECK(cond, ...)                                        \
  do {                                                          \
    if (!(cond)) {                                              \
      std::fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
      std::fprintf(stderr, __VA_ARGS__);                        \
      std::fprintf(stderr, "\n");                               \
      ++failures;                                               \
    }                                                           \
  } while (0)

int main() {
  // pushPull fills the empty half from the weighted half.
  {
    const int w = 16, h = 8;
    std::vector<float> v(w * h, 0.0f), wt(w * h, 0.0f), out;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w / 2; ++x) {
        v[y * w + x] = 0.7f;
        wt[y * w + x] = 1.0f;
      }
    pushPull(v.data(), wt.data(), 1, w, h, out);
    for (size_t i = 0; i < out.size(); ++i) CHECK(std::fabs(out[i] - 0.7f) < 1e-4f, "pushPull %zu %f", i, out[i]);
  }
  // A subject on the left, a soft 40 px edge, and a background with lettering-like detail (a
  // high-contrast checker). The source mixes them in the edge (contaminated edge pixels);
  // decontaminated, every edge pixel is the subject's colour and nothing of the checker is left.
  {
    const int w = 400, h = 120;
    const float F[3] = {0.70f, 0.55f, 0.48f};
    std::vector<float> a(w * h), rgb(w * h * 3), out;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        const float t = std::fmin(1.0f, std::fmax(0.0f, (240.0f - x) / 40.0f));  // 1 left of 200, 0 right of 240
        const bool ink = ((x / 3) + (y / 3)) % 2 == 0;
        const float B[3] = {ink ? 0.9f : 0.05f, ink ? 0.85f : 0.1f, ink ? 0.2f : 0.08f};
        const size_t i = size_t(y) * w + x;
        a[i] = t;
        for (int c = 0; c < 3; ++c) rgb[i * 3 + c] = t * F[c] + (1 - t) * B[c];
      }
    const auto t0 = std::chrono::steady_clock::now();
    decontaminate(rgb.data(), a.data(), w, h, DecontamSettings{}, out);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    double errSrc = 0, errOut = 0, worst = 0;
    int cnt = 0;
    for (int y = 0; y < h; ++y)
      for (int x = 200; x < 240; ++x) {  // the whole soft edge, alpha 1 .. 0.025
        const size_t i = size_t(y) * w + x;
        for (int c = 0; c < 3; ++c) {
          errSrc += std::fabs(rgb[i * 3 + c] - F[c]);
          const double e = std::fabs(out[i * 3 + c] - F[c]);
          errOut += e;
          worst = std::fmax(worst, e);
        }
        ++cnt;
      }
    errSrc /= cnt * 3;
    errOut /= cnt * 3;
    std::printf("soft edge over detail: source off by %.4f, decontaminated off by %.5f, worst %.5f (%.1f ms)\n",
                errSrc, errOut, worst, ms);
    CHECK(worst < 1e-3, "background detail left in the edge: worst %.5f", worst);
    {  // cvt1.62: Edge Blur averages only the subject, so the checker stays out; the deep core stays sharp
      DecontamSettings blur;
      blur.edgeBlur = 6.0f;
      std::vector<float> ob;
      decontaminate(rgb.data(), a.data(), w, h, blur, ob);
      double wb = 0;
      for (int y = 0; y < h; ++y)
        for (int x = 200; x < 240; ++x)
          for (int c = 0; c < 3; ++c) wb = std::fmax(wb, std::fabs(ob[(size_t(y) * w + x) * 3 + c] - F[c]));
      CHECK(wb < 1e-3, "Edge Blur pulled background detail into the edge: worst %.5f", wb);
      for (int x : {0, 100, 190, 300})
        for (int c = 0; c < 3; ++c)
          CHECK(ob[(size_t(60) * w + x) * 3 + c] == rgb[(size_t(60) * w + x) * 3 + c], "Edge Blur changed x=%d", x);
    }
    for (int y = 0; y < h; ++y)  // the solid core and the pure background keep the source
      for (int x : {0, 100, 190, 300, 399}) {
        const size_t i = size_t(y) * w + x;
        for (int c = 0; c < 3; ++c)
          CHECK(out[i * 3 + c] == rgb[i * 3 + c], "changed at x=%d: %f vs %f", x, out[i * 3 + c], rgb[i * 3 + c]);
      }
    // Alpha Erode takes the colour from deeper in: with 20 px, solid pixels near the edge change too.
    DecontamSettings deep;
    deep.erodeWidth = 20.0f;
    for (int y = 0; y < h; ++y)  // a darker band just inside the solid edge
      for (int x = 185; x < 200; ++x)
        for (int c = 0; c < 3; ++c) rgb[(size_t(y) * w + x) * 3 + c] = 0.1f;
    decontaminate(rgb.data(), a.data(), w, h, deep, out);
    const size_t i = size_t(60) * w + 192;
    CHECK(std::fabs(out[i * 3] - F[0]) < 1e-3, "Alpha Erode 20 kept the band inside the edge: %f", out[i * 3]);
    decontaminate(rgb.data(), a.data(), w, h, DecontamSettings{}, out);
    CHECK(out[i * 3] == rgb[i * 3], "default Alpha Erode and Edge Fade changed a solid pixel 8 px inside the edge");
    // cvt1.61: Edge Fade (was Edge Blend) fades from the extended colour to the source, inside the solid part.
    // Solid band of 0.4 from x=100 to 200 (edge at 200): erode 2, blend 20.
    for (int y = 0; y < h; ++y)
      for (int x = 100; x <= 200; ++x)
        for (int c = 0; c < 3; ++c) rgb[(size_t(y) * w + x) * 3 + c] = 0.4f;
    DecontamSettings bl;
    bl.erodeWidth = 2.0f;
    bl.edgeFade = 20.0f;
    decontaminate(rgb.data(), a.data(), w, h, bl, out);
    auto at = [&](int x) { return out[(size_t(60) * w + x) * 3]; };
    // depth = 200 - x + 1 or so; extended colour (0.4, from beyond the blend) at the edge, source deeper
    CHECK(std::fabs(at(199) - 0.4f) < 0.04f, "blend: edge pixel %f", at(199));  // smooth multi-scale average
    std::vector<float> rgb2(rgb);
    for (int y = 0; y < h; ++y)  // make the inner part brighter so the fade shows
      for (int x = 100; x <= 190; ++x) rgb2[(size_t(y) * w + x) * 3] = 0.8f;
    decontaminate(rgb2.data(), a.data(), w, h, bl, out);
    // x=196 (depth ~5): mostly the extended colour (0.8 pushed from depth > 22) but not all;
    // the source there is 0.4. x=170 (depth ~31): the source.
    CHECK(at(196) > 0.6f && at(196) < 0.8f, "blend: x=196 should be between, got %f", at(196));
    CHECK(at(170) == rgb2[(size_t(60) * w + 170) * 3], "blend: x=170 past the blend changed: %f", at(170));
    CHECK(at(215) > 0.75f, "blend: soft edge pixel not the extended colour: %f", at(215));
    // Edge Blur softens the fade: a sharp stripe inside the fade zone gets spread out, deeper stays.
    for (int y = 0; y < h; ++y)
      for (int c = 0; c < 3; ++c) rgb2[(size_t(y) * w + 192) * 3 + c] = 0.0f;
    bl.edgeBlur = 0.0f;
    decontaminate(rgb2.data(), a.data(), w, h, bl, out);
    const float sharp = at(192);
    bl.edgeBlur = 3.0f;
    decontaminate(rgb2.data(), a.data(), w, h, bl, out);
    CHECK(at(192) > sharp + 0.02f, "Edge Blur did not soften a stripe in the fade: %f vs %f", at(192), sharp);
    CHECK(at(170) == rgb2[(size_t(60) * w + 170) * 3], "Edge Blur changed x=170 past the fade: %f", at(170));
  }
  // Faint haze far from the subject keeps the source; an all-solid matte is passed through.
  {
    const int w = 800, h = 64;
    std::vector<float> a(w * h, 0.0f), rgb(w * h * 3, 0.3f), out;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < 40; ++x) a[size_t(y) * w + x] = 1.0f;     // the subject
      for (int x = 600; x < 640; ++x) a[size_t(y) * w + x] = 0.04f;  // haze 560 px away
    }
    for (size_t i = 0; i < size_t(w) * h; ++i) rgb[i * 3] = (i % w) < 40 ? 0.9f : 0.3f;
    decontaminate(rgb.data(), a.data(), w, h, DecontamSettings{}, out);
    CHECK(out[(size_t(30) * w + 620) * 3] == rgb[(size_t(30) * w + 620) * 3], "haze far away was changed");
    std::vector<float> solid(w * h, 1.0f);
    decontaminate(rgb.data(), solid.data(), w, h, DecontamSettings{}, out);
    CHECK(out == rgb, "solid matte changed the picture");
  }
  if (failures == 0) std::printf("test_decontam: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
