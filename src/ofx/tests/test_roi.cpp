// Plain-assert tests for roi; run via ctest.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "roi.h"

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

static bool contains(const RoiBox& r, const RoiBounds& b, int margin, int W, int H) {
  const int mx = std::max(margin, kRoiMinMargin);
  return r.x <= std::max(0, b.x0 - mx) && r.y <= std::max(0, b.y0 - mx) &&
         r.x + r.w >= std::min(W, b.x1 + 1 + mx) && r.y + r.h >= std::min(H, b.y1 + 1 + mx);
}

int main() {
  // Sweep boxes and margins on HD and an odd-sized image.
  for (int W : {1920, 1000, 300}) {
    for (int H : {1080, 540, 200}) {
      for (int margin : {0, 32, 64, 300}) {
        for (int bx : {0, 5, W / 2, W - 3}) {
          for (int by : {0, 7, H / 2, H - 2}) {
            RoiBounds b;
            b.include(bx, by);
            b.include(std::min(W - 1, bx + 40), std::min(H - 1, by + 20));
            const RoiBox r = expandRoi(b, margin, W, H);
            CHECK(r.x >= 0 && r.y >= 0 && r.x + r.w <= W && r.y + r.h <= H,
                  "outside image %dx%d box %d,%d %dx%d", W, H, r.x, r.y, r.w, r.h);
            CHECK(r.w % 32 == 0 || r.w == W, "w=%d not /32 (W=%d)", r.w, W);
            CHECK(r.h % 32 == 0 || r.h == H, "h=%d not /32 (H=%d)", r.h, H);
            CHECK(r.w >= std::min(kRoiMinSide, W) && r.h >= std::min(kRoiMinSide, H),
                  "below minimum %dx%d", r.w, r.h);
            CHECK(contains(r, b, margin, W, H), "box %d,%d %dx%d misses bounds", r.x, r.y, r.w, r.h);
          }
        }
      }
    }
  }
  {  // a big object keeps its margin: 1000 wide + 2*64 -> 1128 -> 1152
    RoiBounds b;
    b.include(400, 300);
    b.include(1399, 800);
    const RoiBox r = expandRoi(b, 64, 1920, 1080);
    CHECK(r.w == 1152 && r.x == 400 - 64 - 12, "wide box x=%d w=%d", r.x, r.w);
    CHECK(r.h == 640 + 0 || r.h == 640, "tall box h=%d", r.h);  // 501+128=629 -> 640
  }
  {  // alpha scan: opaque is not a matte; a matte gives bounds
    RoiScan opaque;
    const float o[8] = {0, 0, 0, 1, 1, 1, 1, 1};
    opaque.addRow(o, 4, 2, 0);
    CHECK(!opaque.alphaVaries, "opaque alpha is not a matte");
    RoiScan m;
    const float row[12] = {0, 0, 0, 0, 0.2f, 0.2f, 0.2f, 0.5f, 1, 1, 1, 1};  // alpha 0, 0.5, 1
    m.addRow(row, 4, 3, 7);
    CHECK(m.alphaVaries && m.alpha.x0 == 1 && m.alpha.x1 == 2 && m.alpha.y0 == 7, "matte bounds");
    RoiScan rgb;
    const float c3[6] = {0, 0, 0, 1, 1, 1};
    rgb.addRow(c3, 3, 2, 0);
    CHECK(!rgb.alphaVaries && !rgb.alpha.any(), "RGB has no matte");
    float v[3];
    roiRowAlpha(row, 4, 3, v);
    CHECK(v[0] == 0.0f && v[1] == 0.5f && v[2] == 1.0f, "alpha values");
    roiRowAlpha(c3, 3, 2, v);
    CHECK(v[0] == 1.0f && v[1] == 1.0f, "RGB alpha is 1");
  }
  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  {  // cvt1.63: guideMask grows the alpha by `grow` px and feathers it
    const int w = 200, h = 200;
    std::vector<float> a(w * h, 0.0f), g;
    for (int y = 90; y < 110; ++y)
      for (int x = 90; x < 110; ++x) a[y * w + x] = 0.8f;  // a 20 px square, soft value over 0.5
    guideMask(a.data(), w, h, 20.0f, 10.0f, g);
    auto at = [&](int x, int y) { return g[y * w + x]; };
    CHECK(at(100, 100) > 0.99f, "guide centre %f", at(100, 100));
    CHECK(at(120, 100) > 0.7f, "guide 10 px outside the square %f", at(120, 100));
    CHECK(at(129, 100) > 0.3f && at(129, 100) < 0.7f, "guide at the grown edge %f", at(129, 100));
    CHECK(at(160, 100) < 0.01f, "guide far outside %f", at(160, 100));
    std::vector<float> z(w * h, 0.3f);  // nothing at or above 0.5: an empty guide
    guideMask(z.data(), w, h, 20.0f, 10.0f, g);
    float mx = 0;
    for (float v : g) mx = std::max(mx, v);
    CHECK(mx == 0.0f, "guide from an alpha below 0.5 is not empty: %f", mx);
  }
  {  // cvt1.65: upscaleInt keeps flat areas, and a k x k mean of the result gives the input back closely
    const int w = 40, h = 30, k = 3;
    std::vector<float> in(w * h * 2), up;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        in[(y * w + x) * 2] = 0.25f;
        in[(y * w + x) * 2 + 1] = (x / 10 + y / 10) % 2 ? 0.9f : 0.1f;  // blocks
      }
    upscaleInt(in.data(), w, h, 2, k, up);
    CHECK(up.size() == size_t(w * k * h * k * 2), "upscale size");
    double worstFlat = 0, worstBack = 0;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        double m = 0;
        for (int dy = 0; dy < k; ++dy)
          for (int dx = 0; dx < k; ++dx) {
            const size_t o = (size_t(y * k + dy) * w * k + x * k + dx) * 2;
            worstFlat = std::max(worstFlat, double(std::fabs(up[o] - 0.25f)));
            m += up[o + 1];
          }
        m /= k * k;
        if (x % 10 > 1 && x % 10 < 8 && y % 10 > 1 && y % 10 < 8)  // away from block edges
          worstBack = std::max(worstBack, std::fabs(m - in[(y * w + x) * 2 + 1]));
      }
    CHECK(worstFlat < 1e-5, "upscale changed a flat channel: %f", worstFlat);
    CHECK(worstBack < 1e-3, "k x k mean of the upscale is not the input: %f", worstBack);
    CHECK(cropScaleFor(3, 640, 360, kCropScaleMaxPixels) == 3, "640x360 at 3x fits HD");
    CHECK(cropScaleFor(3, 700, 450, kCropScaleMaxPixels) == 2, "700x450 steps down to 2x");
    CHECK(cropScaleFor(3, 1600, 900, kCropScaleMaxPixels) == 1, "1600x900 is not scaled");
  }
  if (failures == 0) std::printf("test_roi: all checks passed\n");  // cvt1.63: failures now fail the test
  return failures == 0 ? 0 : 1;
}
