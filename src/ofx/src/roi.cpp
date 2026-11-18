#include "roi.h"

#include <algorithm>
#include <cmath>

namespace ltxag {

void RoiBounds::include(int x, int y) {
  x0 = std::min(x0, x);
  y0 = std::min(y0, y);
  x1 = std::max(x1, x);
  y1 = std::max(y1, y);
}

void RoiScan::addRow(const float* px, int comps, int w, int y) {
  if (comps != 4) return;  // no alpha: opaque, not a matte
  for (int x = 0; x < w; ++x) {
    const float a = px[size_t(x) * 4 + 3];
    if (a < 0.99f) alphaVaries = true;
    if (a > kRoiThreshold) alpha.include(x, y);
  }
}

void roiRowAlpha(const float* px, int comps, int w, float* out) {
  for (int x = 0; x < w; ++x)
    out[x] = comps == 4 ? std::min(1.0f, std::max(0.0f, px[size_t(x) * 4 + 3])) : 1.0f;
}

// [lo, hi] inclusive -> start and length along one axis of size `size`.
static void fitAxis(int lo, int hi, int margin, int size, int& start, int& len) {
  int a = std::max(0, lo - margin), b = std::min(size, hi + 1 + margin);  // [a, b)
  int want = std::max(b - a, std::min(kRoiMinSide, size));
  want = (want + kRoiMultiple - 1) / kRoiMultiple * kRoiMultiple;
  want = std::min(want, size);
  start = (a + b) / 2 - want / 2;  // centred on the grown box, then slid into the image
  start = std::max(0, std::min(start, size - want));
  len = want;
}

RoiBox expandRoi(const RoiBounds& b, int margin, int imgW, int imgH) {
  RoiBox r{};
  fitAxis(b.x0, b.x1, std::max(margin, kRoiMinMargin), imgW, r.x, r.w);
  fitAxis(b.y0, b.y1, std::max(margin, kRoiMinMargin), imgH, r.y, r.h);
  return r;
}

void guideMask(const float* alpha, int w, int h, float grow, float feather, std::vector<float>& out) {
  const size_t n = size_t(w) * h;
  out.assign(n, 0.0f);
  if (n == 0) return;
  // Distance (chamfer 1 / sqrt 2) to the nearest pixel at or above 0.5; within `grow` is inside.
  const float inf = 1e30f, d1 = 1.0f, d2 = 1.41421356f;
  std::vector<float> d(n);
  for (size_t i = 0; i < n; ++i) d[i] = alpha[i] >= 0.5f ? 0.0f : inf;
  auto at = [&](int x, int y) -> float& { return d[size_t(y) * w + x]; };
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      float v = at(x, y);
      if (x > 0) v = std::min(v, at(x - 1, y) + d1);
      if (y > 0) {
        v = std::min(v, at(x, y - 1) + d1);
        if (x > 0) v = std::min(v, at(x - 1, y - 1) + d2);
        if (x + 1 < w) v = std::min(v, at(x + 1, y - 1) + d2);
      }
      at(x, y) = v;
    }
  for (int y = h - 1; y >= 0; --y)
    for (int x = w - 1; x >= 0; --x) {
      float v = at(x, y);
      if (x + 1 < w) v = std::min(v, at(x + 1, y) + d1);
      if (y + 1 < h) {
        v = std::min(v, at(x, y + 1) + d1);
        if (x + 1 < w) v = std::min(v, at(x + 1, y + 1) + d2);
        if (x > 0) v = std::min(v, at(x - 1, y + 1) + d2);
      }
      at(x, y) = v;
    }
  for (size_t i = 0; i < n; ++i) out[i] = d[i] <= grow ? 1.0f : 0.0f;
  // Feather: three box passes of radius r (about a Gaussian of sigma r), rows then columns.
  const int r = int(std::lround(std::max(0.0f, feather)));
  if (r <= 0) return;
  std::vector<double> pre;
  std::vector<float> tmp(n);
  auto pass = [&](const std::vector<float>& in, std::vector<float>& o, int lines, int len, size_t ls, size_t st) {
    pre.assign(size_t(len) + 1, 0.0);
    for (int l = 0; l < lines; ++l) {
      const size_t base = l * ls;
      for (int i = 0; i < len; ++i) pre[i + 1] = pre[i] + in[base + i * st];
      for (int i = 0; i < len; ++i) {
        const int a = std::max(0, i - r), b = std::min(len, i + r + 1);
        o[base + i * st] = float((pre[b] - pre[a]) / (b - a));
      }
    }
  };
  for (int k = 0; k < 3; ++k) {
    pass(out, tmp, h, w, size_t(w), 1);
    pass(tmp, out, w, h, 1, size_t(w));
  }
}

namespace {
inline float catmullRom(float t) {  // a = -0.5
  t = std::fabs(t);
  if (t < 1.0f) return 1.5f * t * t * t - 2.5f * t * t + 1.0f;
  if (t < 2.0f) return -0.5f * t * t * t + 2.5f * t * t - 4.0f * t + 2.0f;
  return 0.0f;
}
// One axis: `lines` lines of `len` samples (stride `step`, `ls` between lines) -> len*k samples.
void upAxis(const std::vector<float>& in, std::vector<float>& out, int lines, int len, size_t ls, size_t step,
            size_t ols, size_t ostep, int ch, int k) {
  for (int o = 0; o < len * k; ++o) {
    const float src = (o + 0.5f) / k - 0.5f;  // pixel centres line up (the inverse is a k x k mean)
    const int i0 = int(std::floor(src));
    float wts[4];
    int idx[4];
    float sum = 0.0f;
    for (int j = 0; j < 4; ++j) {
      idx[j] = std::min(len - 1, std::max(0, i0 - 1 + j));
      wts[j] = catmullRom(src - float(i0 - 1 + j));
      sum += wts[j];
    }
    for (int l = 0; l < lines; ++l)
      for (int c = 0; c < ch; ++c) {
        float v = 0.0f;
        for (int j = 0; j < 4; ++j) v += wts[j] * in[l * ls + idx[j] * step + c];
        out[l * ols + o * ostep + c] = v / sum;
      }
  }
}
}  // namespace

void upscaleInt(const float* in, int w, int h, int ch, int k, std::vector<float>& out) {
  if (k <= 1) {
    out.assign(in, in + size_t(w) * h * ch);
    return;
  }
  std::vector<float> src(in, in + size_t(w) * h * ch), mid(size_t(w) * k * h * ch);
  out.assign(size_t(w) * k * h * k * ch, 0.0f);
  upAxis(src, mid, h, w, size_t(w) * ch, size_t(ch), size_t(w) * k * ch, size_t(ch), ch, k);  // rows
  upAxis(mid, out, w * k, h, size_t(ch), size_t(w) * k * ch, size_t(ch), size_t(w) * k * ch, ch, k);  // columns
}

int cropScaleFor(int want, int w, int h, double maxPixels) {
  int k = std::max(1, want);
  while (k > 1 && double(w) * k * double(h) * k > maxPixels) --k;
  return k;
}

}  // namespace ltxag
