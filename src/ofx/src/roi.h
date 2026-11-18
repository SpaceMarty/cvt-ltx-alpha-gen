// Region of interest from the source's own alpha (decision 63). Pure functions, no OFX.
// The box is the union of the alpha matte over the whole clip, grown by a margin, grown again to a
// minimum size, and rounded up to a multiple of 32; the model then sees only that crop.
#pragma once

#include <climits>
#include <vector>

namespace ltxag {

constexpr float kRoiThreshold = 0.05f;  // matte value above which a pixel counts as inside
constexpr int kRoiMinMargin = 32;       // pixels of context around the matte, at least
constexpr int kRoiMinSide = 512;        // smallest crop side (the model is poor on tiny frames)
constexpr int kRoiMultiple = 32;        // crop sides are multiples of this (or the image side)

struct RoiBounds {
  int x0 = INT_MAX, y0 = INT_MAX, x1 = -1, y1 = -1;  // inclusive, top-down rows
  bool any() const { return x1 >= 0; }
  void include(int x, int y);
};

// Union bounds of the pixels whose alpha is above the threshold, and whether the alpha is a
// matte at all (any value below 0.99), over every row added.
struct RoiScan {
  RoiBounds alpha;
  bool alphaVaries = false;
  // One image row of interleaved floats (comps 3 or 4; 3 has no alpha), y top-down.
  void addRow(const float* px, int comps, int w, int y);
};

// A row of alpha values (0..1); 1 where the image has no alpha.
void roiRowAlpha(const float* px, int comps, int w, float* out);

struct RoiBox {
  int x, y, w, h;  // top-down, inside the image
};

// bounds must be non-empty. Result lies inside [0,imgW) x [0,imgH); sides are multiples of 32
// unless that would exceed the image.
RoiBox expandRoi(const RoiBounds& bounds, int margin, int imgW, int imgH);

// cvt1.63: the guide mask for the model (Guide With Alpha): the input alpha (w*h, rows in any
// order) thresholded at 0.5, grown by `grow` px (round) and feathered (about a Gaussian of
// `feather` px), as the subject mask that steers LTX's attention. A hard mask cuts hair; grown
// and feathered, LTX's own edge decides the outline.
constexpr float kGuideGrow = 20.0f, kGuideFeather = 10.0f;  // px at 1080p (scaled with the frame)
void guideMask(const float* alpha, int w, int h, float grow, float feather, std::vector<float>& out);

// cvt1.65: Crop Scale. The crop is scaled up by a whole factor k (Catmull-Rom, separable) so a
// small subject covers more of the model's 32 px blocks; the matte comes back k times larger and
// is averaged back down over k x k blocks (job.py). in: w*h*ch floats, rows in any order.
void upscaleInt(const float* in, int w, int h, int ch, int k, std::vector<float>& out);
// The largest factor <= want for which the scaled crop stays within maxPixels (at least 1).
int cropScaleFor(int want, int w, int h, double maxPixels);
constexpr double kCropScaleMaxPixels = 1920.0 * 1088.0;  // about HD: a scaled crop is never bigger

}  // namespace ltxag
