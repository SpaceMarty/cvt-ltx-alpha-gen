#include "vram_estimate.h"

#include <algorithm>

namespace ltxag {

namespace {
constexpr double kBaseFp8 = 19.5;
constexpr double kBf16Extra = 17.3;
constexpr double kPerMpxFrame = 0.0718;
constexpr double kPerMpxAboveHD = 0.70;
constexpr double kHDMpx = 1920.0 * 1088.0 / 1e6;
constexpr double kMargin = 1.0;
constexpr double kHeldFp8 = 21.7;   // reserved with the transformer kept (A/B run, HD, frames streamed)
constexpr double kHeldBf16 = 36.8;  // same, bf16 (Flame test 2)
constexpr double kEncodeBase = 1.56;
constexpr double kEncodePerMpxFrame = 0.1088;
constexpr double kContext = 0.7;  // CUDA context, outside torch's numbers
constexpr double kMpsBase = 44.5;  // M1 Ultra fit, see the header
constexpr double kMpsPerMpxFrame = 0.231;
// Apple Silicon at 16 bits per weight: M5 and newer in bf16, an M1-class Mac on the fp16 copy
// (CVT port, cvt1.28), see the header.
constexpr double kMps16Floor = 40.8;
constexpr double kMps16Base = 37.0;
constexpr double kMps16PerMpxFrame = 0.10;
// cvt1.51: the worker loads the transformer without its audio branch (cvt1.50), so every Apple
// Silicon fit drops by a fixed amount (the per-frame growth did not change). cvt1.52: by 7.0 GB, the
// smallest drop on any chunk measured on the M5 Max (HD 73f 50.2 -> 38.3-39.1 GB on most chunks, but
// 43.1 GB on one first chunk; 768x432 17f 40.4 -> 27.4 GB). cvt1.51's 11.3 GB under-estimated that
// chunk by 1.1 GB.
constexpr double kMpsNoAudio = 7.0;
// cvt1.52: and never below the highest peak measured without audio, whatever the chunk size: 43.1 GB
// (HD 73f first chunk in Resolve; HD 33f in worker_smoke took 42.9). That peak looks like the model
// load, not the frames.
constexpr double kMpsNoAudioFloor = 43.1;
// cvt1.53, refit cvt1.54: 8-bit weights (Weights 8-bit, or Auto when 16-bit doesn't fit): the transformer drops from
// 26.3 to 13.9 GB. worker_smoke (transformer kept) saw 12.0 GB less (HD 33f 42.9 -> 30.9 GB), but a
// Resolve job less: HD 65f in one chunk peaked at 34.3 GB reserved (16-bit HD 73f: 43.1 on a first
// chunk), about 9 GB less. Every Apple Silicon need is 8.5 GB lower (11.5 under-estimated that job).
constexpr double kMpsInt8Saving = 8.5;  // cvt1.55: now only for the M1 fit (mps 1)
// cvt1.55: the reference is encoded in tiles (worker, cvt1.55), which took the encode from the peak
// (HD 65f: 38.6 -> 9.6 GB reserved). Refit for the M5 and newer (mps 2, 3) from peak reserved MPS memory,
// worker_smoke with the transformer rebuilt per chunk as in a host (M5 Max, video-only, tiled encode):
//   16-bit: 768x432 17f 25.8, HD 33f 31.2, HD 65f 34.9, HD 73f 36.4 GB
//   8-bit:  768x432 17f 13.5, HD 33f 19.2, HD 65f 28.7, HD 73f 33.6 GB (the decode's cache grows on
//           long chunks: it allocates 12.1 GB and holds up to 33.6)
// need = base + slope * Mpx * frames + 1 GB.
constexpr double kMps16TiledBase = 26.5, kMps16TiledPerMpxFrame = 0.075;
constexpr double kMps8TiledBase = 13.0, kMps8TiledPerMpxFrame = 0.137;
// cvt1.56: 16-bit runs the full model again (worker). Same runs as cvt1.55 with the audio branch
// loaded: 768x432 17f 40.6, HD 33f 42.7, HD 65f 48.4, HD 73f 49.9 GB reserved. The extra is the
// audio weights (allocated barely moves; a sync per tensor while loading changed nothing). Small chunks
// sit on the load's peak, hence the floor.
constexpr double kMpsFullFloor = 40.6, kMpsFullBase = 36.8, kMpsFullPerMpxFrame = 0.086;
constexpr double kStreamBase = 6.0;  // Windows port: weights streamed to the GPU, see the header
}  // namespace

VramNeed estimateVramNeed(bool fp8, int width, int height, int frames, bool residentTransformer, int mps,
                          Weights weights, bool streamed) {
  const bool int8 = weights == Weights::Int8;
  VramNeed n{};
  n.modelW = (std::max(width, 1) + 31) / 32 * 32;
  n.modelH = (std::max(height, 1) + 31) / 32 * 32;
  const int f = std::min(std::max(frames, 1), 145);
  n.modelFrames = f + (8 - (f - 1) % 8) % 8;
  const double mpx = double(n.modelW) * double(n.modelH) / 1e6;
  n.overMpsLimit = mps != 0 && mpx * n.modelFrames > kMpsMaxMpxFrames;
  if (mps >= 2) {  // cvt1.28: one fit for the M5 (bf16) and an M1 on the fp16 copy
    // cvt1.55: the tiled-encode fits (above); the older fits stay as the record of how we got here.
    (void)kMps16Floor, (void)kMps16Base, (void)kMps16PerMpxFrame, (void)kMpsNoAudioFloor;
    const double x = mpx * n.modelFrames;
    if (int8)
      n.gb = kMps8TiledBase + kMps8TiledPerMpxFrame * x + kMargin;
    else if (weights == Weights::VideoOnly16)
      n.gb = kMps16TiledBase + kMps16TiledPerMpxFrame * x + kMargin;
    else  // cvt1.56
      n.gb = std::max(kMpsFullFloor, kMpsFullBase + kMpsFullPerMpxFrame * x) + kMargin;
    return n;
  }
  if (mps) {
    n.gb = kMpsBase + kMpsPerMpxFrame * mpx * n.modelFrames + kMargin;  // the full model (cvt1.56 again)
    if (weights != Weights::Full16) n.gb -= kMpsNoAudio;  // cvt1.51 (not measured on an M1)
    if (int8) n.gb -= kMpsInt8Saving;  // cvt1.53 (not measured on an M1)
    return n;
  }
  if (streamed) {  // Windows port
    n.gb = std::max(kStreamBase + kPerMpxFrame * mpx * n.modelFrames,
                    kEncodeBase + kContext + kEncodePerMpxFrame * mpx * n.modelFrames) +
           kPerMpxAboveHD * std::max(0.0, mpx - kHDMpx) + kMargin;
    return n;
  }
  n.gb = kBaseFp8 + (fp8 ? 0.0 : kBf16Extra) + kPerMpxFrame * mpx * n.modelFrames +
         kPerMpxAboveHD * std::max(0.0, mpx - kHDMpx) + kMargin;
  if (residentTransformer) {
    const double withHeld = (fp8 ? kHeldFp8 : kHeldBf16) + kEncodeBase + kEncodePerMpxFrame * mpx * n.modelFrames +
                            kContext + kMargin;
    n.gb = std::max(n.gb, withHeld);
  }
  return n;
}

}  // namespace ltxag
