#include "decontam.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ltxag {

namespace {

inline float smooth01(float t) {
  t = std::min(1.0f, std::max(0.0f, t));
  return t * t * (3.0f - 2.0f * t);
}

// Chamfer (3-4) distance in pixels to the nearest seed; +inf where there is none.
std::vector<float> distanceTo(const std::vector<uint8_t>& seed, int w, int h) {
  const float inf = std::numeric_limits<float>::infinity();
  std::vector<float> d(seed.size());
  for (size_t i = 0; i < d.size(); ++i) d[i] = seed[i] ? 0.0f : inf;
  const float a = 1.0f, b = 1.41421356f;
  auto at = [&](int x, int y) -> float& { return d[size_t(y) * w + x]; };
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      float v = at(x, y);
      if (x > 0) v = std::min(v, at(x - 1, y) + a);
      if (y > 0) {
        v = std::min(v, at(x, y - 1) + a);
        if (x > 0) v = std::min(v, at(x - 1, y - 1) + b);
        if (x + 1 < w) v = std::min(v, at(x + 1, y - 1) + b);
      }
      at(x, y) = v;
    }
  for (int y = h - 1; y >= 0; --y)
    for (int x = w - 1; x >= 0; --x) {
      float v = at(x, y);
      if (x + 1 < w) v = std::min(v, at(x + 1, y) + a);
      if (y + 1 < h) {
        v = std::min(v, at(x, y + 1) + a);
        if (x + 1 < w) v = std::min(v, at(x + 1, y + 1) + b);
        if (x > 0) v = std::min(v, at(x - 1, y + 1) + b);
      }
      at(x, y) = v;
    }
  return d;
}

// cvt1.62: one box pass of radius r along rows then columns, ch channels, in place (prefix sums;
// pixels past the image edge don't count, so the borders don't darken).
void boxPass(std::vector<float>& v, int ch, int w, int h, int r) {
  std::vector<double> pre;
  std::vector<float> tmp(v.size());
  auto axis = [&](const std::vector<float>& in, std::vector<float>& out, int lines, int len, size_t lineStride,
                  size_t step) {
    pre.assign(size_t(len) + 1, 0.0);
    for (int l = 0; l < lines; ++l)
      for (int c = 0; c < ch; ++c) {
        const size_t base = l * lineStride + c;
        for (int i = 0; i < len; ++i) pre[i + 1] = pre[i] + in[base + i * step];
        for (int i = 0; i < len; ++i) {
          const int a = std::max(0, i - r), b = std::min(len, i + r + 1);
          out[base + i * step] = float((pre[b] - pre[a]) / (b - a));
        }
      }
  };
  axis(v, tmp, h, w, size_t(w) * ch, size_t(ch));
  axis(tmp, v, w, h, size_t(ch), size_t(w) * ch);
}

}  // namespace

void pushPull(const float* val, const float* weight, int ch, int w, int h, std::vector<float>& out) {
  struct Level {
    int w, h;
    std::vector<float> v, wt;  // v premultiplied by wt
  };
  std::vector<Level> lv(1);
  lv[0].w = w;
  lv[0].h = h;
  lv[0].v.resize(size_t(w) * h * ch);
  lv[0].wt.assign(weight, weight + size_t(w) * h);
  for (size_t i = 0; i < size_t(w) * h; ++i)
    for (int c = 0; c < ch; ++c) lv[0].v[i * ch + c] = val[i * ch + c] * weight[i];
  while (lv.back().w > 1 || lv.back().h > 1) {  // pull: 2x2 averages
    const Level& f = lv.back();
    Level g;
    g.w = (f.w + 1) / 2;
    g.h = (f.h + 1) / 2;
    g.v.assign(size_t(g.w) * g.h * ch, 0.0f);
    g.wt.assign(size_t(g.w) * g.h, 0.0f);
    for (int y = 0; y < g.h; ++y)
      for (int x = 0; x < g.w; ++x) {
        int cnt = 0;
        const size_t gi = size_t(y) * g.w + x;
        for (int dy = 0; dy < 2; ++dy)
          for (int dx = 0; dx < 2; ++dx) {
            const int fx = 2 * x + dx, fy = 2 * y + dy;
            if (fx >= f.w || fy >= f.h) continue;
            const size_t fi = size_t(fy) * f.w + fx;
            g.wt[gi] += f.wt[fi];
            for (int c = 0; c < ch; ++c) g.v[gi * ch + c] += f.v[fi * ch + c];
            ++cnt;
          }
        g.wt[gi] /= cnt;
        for (int c = 0; c < ch; ++c) g.v[gi * ch + c] /= cnt;
      }
    lv.push_back(std::move(g));
  }
  // push: from the coarsest level down, trusting each level where it has enough weight
  std::vector<float> cur(ch);
  {
    const Level& t = lv.back();
    cur.resize(size_t(t.w) * t.h * ch);
    for (size_t i = 0; i < size_t(t.w) * t.h; ++i)
      for (int c = 0; c < ch; ++c) cur[i * ch + c] = t.wt[i] > 1e-6f ? t.v[i * ch + c] / t.wt[i] : 0.0f;
  }
  for (int l = int(lv.size()) - 2; l >= 0; --l) {
    const Level& f = lv[size_t(l)];
    const Level& g = lv[size_t(l) + 1];
    std::vector<float> next(size_t(f.w) * f.h * ch);
    for (int y = 0; y < f.h; ++y) {
      const float gy = std::min(float(g.h - 1), std::max(0.0f, (y + 0.5f) * 0.5f - 0.5f));
      const int y0 = int(gy), y1 = std::min(g.h - 1, y0 + 1);
      const float ty = gy - y0;
      for (int x = 0; x < f.w; ++x) {
        const float gx = std::min(float(g.w - 1), std::max(0.0f, (x + 0.5f) * 0.5f - 0.5f));
        const int x0 = int(gx), x1 = std::min(g.w - 1, x0 + 1);
        const float tx = gx - x0;
        const size_t i = size_t(y) * f.w + x;
        const float trust = std::min(1.0f, f.wt[i] * 4.0f);
        for (int c = 0; c < ch; ++c) {
          auto C = [&](int xx, int yy) { return cur[(size_t(yy) * g.w + xx) * ch + c]; };
          const float up = (C(x0, y0) * (1 - tx) + C(x1, y0) * tx) * (1 - ty) +
                           (C(x0, y1) * (1 - tx) + C(x1, y1) * tx) * ty;
          const float here = f.wt[i] > 1e-6f ? f.v[i * ch + c] / f.wt[i] : 0.0f;
          next[i * ch + c] = trust * here + (1.0f - trust) * up;
        }
      }
    }
    cur.swap(next);
  }
  out.swap(cur);
}

void decontaminate(const float* rgb, const float* alpha, int w, int h, const DecontamSettings& s,
                   std::vector<float>& out) {
  const size_t n = size_t(w) * h;
  out.assign(rgb, rgb + n * 3);
  if (n == 0) return;
  const float sc = std::max(0.25f, s.scale);
  const float erode = std::max(0.0f, s.erodeWidth);
  const float blend = std::max(1.5f, s.edgeFade);  // cvt1.61 Edge Blend, cvt1.62 Edge Fade; 1.5 px = cvt1.60's step

  // cvt1.60: strict edge extend. The solid core (matte >= 0.98), eroded by Alpha Erode, keeps the
  // source; every other pixel of the subject gets the core's colour pushed outwards and nothing of
  // its own, so no detail of the old background (texture, lettering) survives in soft edges.
  // cvt1.61: Edge Fade (called Edge Blend in 1.61) crossfades from the extended colour to the source
  // over that many pixels further in, inside the solid part, so the change is gradual and still
  // brings no background back. The colour is taken from beyond the fade (else it would equal the
  // source there). cvt1.62: Edge Blur then blurs that result near the edge (see below).
  std::vector<uint8_t> notSolid(n);
  bool anySolid = false, anyEdge = false;
  for (size_t i = 0; i < n; ++i) {
    notSolid[i] = alpha[i] < kDecontamCore;
    anySolid |= !notSolid[i];
    anyEdge |= notSolid[i] && alpha[i] >= kDecontamBg;
  }
  if (!anySolid || !anyEdge) return;
  const std::vector<float> depth = distanceTo(notSolid, w, h);  // how far inside the solid core
  std::vector<float> core(n);
  std::vector<uint8_t> inCore(n);
  bool anyCore = false;
  for (size_t i = 0; i < n; ++i) {
    inCore[i] = depth[i] > erode + blend;
    core[i] = inCore[i] ? 1.0f : 0.0f;
    anyCore |= inCore[i] != 0;
  }
  if (!anyCore) return;  // eroded away entirely: nothing to take the colour from
  std::vector<float> C;
  pushPull(rgb, core.data(), 3, w, h, C);

  // Faint matte haze far from the subject (around out-of-focus lights) is not the subject's edge:
  // it stays as it was. Pure background (matte < 0.02) also stays (it shows with Post-Multiply off).
  const std::vector<float> fromCore = distanceTo(inCore, w, h);
  const float reach = kDecontamReach * sc + erode + blend;
  std::vector<float> wgt(n, 0.0f);
  for (size_t i = 0; i < n; ++i) {
    if (depth[i] >= erode + blend) continue;
    const float edgeIn = 1.0f - smooth01((depth[i] - erode) / blend);  // 1 out to Alpha Erode, 0 past the fade
    const float near = 1.0f - smooth01((fromCore[i] - 0.75f * reach) / (0.25f * reach));
    wgt[i] = edgeIn * near * smooth01(alpha[i] / kDecontamBg);
    if (wgt[i] <= 0.0f) continue;
    for (int c = 0; c < 3; ++c) out[i * 3 + c] = rgb[i * 3 + c] + (C[i * 3 + c] - rgb[i * 3 + c]) * wgt[i];
  }

  // cvt1.62: Edge Blur, the last step on the RGB. A blur (three box passes, about a Gaussian of that
  // many pixels) of the result above, averaging only the subject's pixels (matte >= 0.02, near the
  // subject): the old background next to the edge is never pulled back in. It applies where the
  // extended colour does (the soft edge, Alpha Erode and the fade) and tapers out with the fade, so
  // the picture deeper in stays sharp.
  const int br = int(std::lround(std::max(0.0f, s.edgeBlur)));
  if (br <= 0) return;
  std::vector<float> m(n), mv(n * 3);
  for (size_t i = 0; i < n; ++i) {
    m[i] = (alpha[i] >= kDecontamBg && fromCore[i] <= reach) ? 1.0f : 0.0f;
    for (int c = 0; c < 3; ++c) mv[i * 3 + c] = out[i * 3 + c] * m[i];
  }
  for (int pass = 0; pass < 3; ++pass) {
    boxPass(mv, 3, w, h, br);
    boxPass(m, 1, w, h, br);
  }
  for (size_t i = 0; i < n; ++i) {
    if (wgt[i] <= 0.0f || m[i] < 1e-4f) continue;
    for (int c = 0; c < 3; ++c) {
      const float b = mv[i * 3 + c] / m[i];
      out[i * 3 + c] += (b - out[i * 3 + c]) * wgt[i];
    }
  }
}

}  // namespace ltxag
