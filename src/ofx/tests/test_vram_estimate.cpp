// The estimate must cover every measured peak (never under-estimate) without being wildly high.
#include <cmath>
#include <cstdio>

#include "vram_estimate.h"

using namespace ltxag;

static int failures = 0;
// cvt1.51: the M1 Ultra points below were measured with the audio branch loaded (up to cvt1.49);
// without it (cvt1.50) the same jobs need at least 7.0 GB less (the smallest drop on the M5 Max).
static constexpr double kNoAudio = 7.0;

static void covers(const char* what, bool fp8, int w, int h, int f, bool resident, double measured,
                   double maxOver, int mps = 0, Weights wts = Weights::Full16) {
  const VramNeed n = estimateVramNeed(fp8, w, h, f, resident, mps, wts);
  const bool ok = n.gb >= measured && n.gb - measured <= maxOver;
  std::printf("%s %-34s est %5.1f GB vs measured %5.1f GB (model %dx%dx%d)\n", ok ? "ok  " : "FAIL", what, n.gb,
              measured, n.modelW, n.modelH, n.modelFrames);
  if (!ok) ++failures;
}

int main() {
  // Official CLI measurements (HD sizes as the model sees them).
  covers("HD 97 fp8", true, 1920, 1088, 97, false, 34.1, 4.0);
  covers("HD 145 fp8", true, 1920, 1088, 145, false, 41.3, 4.0);
  covers("HD 97 bf16", false, 1920, 1088, 97, false, 51.7, 4.0);
  covers("HD 145 bf16", false, 1920, 1088, 145, false, 58.8, 4.0);
  covers("UHD 49 fp8", true, 3840, 2176, 49, false, 54.1, 4.0);
  covers("UHD 49 bf16", false, 3840, 2176, 49, false, 67.8, 4.0);
  // Our worker (peak allocated + ~1.5 GB CUDA context/reserve slack).
  covers("worker HD 60->65 fp8", true, 1920, 1080, 60, false, 28.3 + 1.5, 4.0);
  covers("worker HD 60->65 bf16", false, 1920, 1080, 60, false, 45.5 + 1.5, 4.0);
  covers("worker HD 60->65 fp8 resident", true, 1920, 1080, 60, true, 37.7 + 1.5, 4.0);
  // Transformer kept (resident), measured as reserved + 0.7 GB context = what free memory lost.
  covers("A/B HD 145 fp8 resident, chunk 2", true, 1920, 1080, 145, true, 56.2 + 0.7, 4.0);
  covers("test 2: 720p 57 bf16 resident reused", false, 1280, 720, 50, true, 44.2, 4.0);
  covers("test 2: 720p 57 fp8 resident fresh", true, 1280, 720, 50, true, 28.0, 4.0);
  // Apple Silicon, M1 Ultra, our worker (decision 65): lowest free working set.
  covers("mps 720p 49 bf16", false, 1280, 720, 49, false, 52.3 - kNoAudio, 4.0, 1, Weights::VideoOnly16);
  covers("mps HD 33 bf16", false, 1920, 1080, 33, false, 60.4 - kNoAudio, 4.0, 1, Weights::VideoOnly16);
  covers("mps HD 73 bf16", false, 1920, 1080, 73, false, 79.7 - kNoAudio, 4.0, 1, Weights::VideoOnly16);
  // cvt1.55: the M5-and-newer fit (mps 2, and 3 for an M1 on the fp16 copy) after the tiled encode:
  // peak reserved, worker_smoke with the transformer rebuilt per chunk, M5 Max, video-only. The older
  // M5 and M1 Max points (audio loaded, encode in one piece) no longer apply and are in git history.
  covers("mps M5 768x432 17 16-bit", false, 768, 432, 17, false, 25.8, 2.5, 2, Weights::VideoOnly16);
  covers("mps M5 HD 33 16-bit", false, 1920, 1080, 33, false, 31.2, 2.5, 2, Weights::VideoOnly16);
  covers("mps M5 HD 65 16-bit", false, 1920, 1080, 64, false, 34.9, 3.0, 2, Weights::VideoOnly16);
  covers("mps M5 HD 73 16-bit", false, 1920, 1080, 73, false, 36.4, 3.0, 2, Weights::VideoOnly16);
  covers("mps M5 768x432 17 8-bit", false, 768, 432, 17, false, 13.5, 2.5, 2, Weights::Int8);
  covers("mps M5 HD 33 8-bit", false, 1920, 1080, 33, false, 19.2, 4.5, 2, Weights::Int8);
  covers("mps M5 HD 65 8-bit", false, 1920, 1080, 64, false, 28.7, 4.5, 2, Weights::Int8);
  covers("mps M5 HD 73 8-bit", false, 1920, 1080, 73, false, 33.6, 2.5, 2, Weights::Int8);
  // cvt1.56: the full model (audio branch loaded) again for 16-bit, M5 Max, tiled encode.
  covers("mps M5 768x432 17 full", false, 768, 432, 17, false, 40.6, 2.5, 2);
  covers("mps M5 HD 33 full", false, 1920, 1080, 33, false, 42.7, 2.5, 2);
  covers("mps M5 HD 65 full", false, 1920, 1080, 64, false, 48.4, 2.5, 2);
  covers("mps M5 HD 73 full", false, 1920, 1080, 73, false, 49.9, 2.5, 2);
  // The M1 Ultra fit (mps 1) was measured with the audio branch.
  covers("mps M1 720p 49 full", false, 1280, 720, 49, false, 52.3, 4.0, 1);
  covers("mps M1 HD 73 full", false, 1920, 1080, 73, false, 79.7, 4.0, 1);
  // cvt1.56: M1 Max (64 GB) runs in Resolve on cvt1.55: fp16 copy video-only (mps 3), and 8-bit.
  covers("mps M1 Max HD 33 video-only", false, 1920, 1080, 30, false, 31.8, 2.5, 3, Weights::VideoOnly16);
  covers("mps M1 Max HD 73 video-only", false, 1920, 1080, 73, false, 36.9, 2.5, 3, Weights::VideoOnly16);
  covers("mps M1 Max HD 33 8-bit", false, 1920, 1080, 32, false, 19.4, 4.5, 3, Weights::Int8);
  // The fp16 copy on an M1 uses the same fit (not measured since cvt1.28).
  if (estimateVramNeed(false, 1920, 1080, 33, false, 3).gb != estimateVramNeed(false, 1920, 1080, 33, false, 2).gb) {
    std::printf("FAIL mps 3 and 2 differ\n");
    ++failures;
  }
  // cvt1.28: the MPS tensor limit: HD 81 and UHD 17 ran, HD 145 failed (INT_MAX).
  if (estimateVramNeed(false, 1920, 1080, 81, false, 2).overMpsLimit ||
      estimateVramNeed(false, 3840, 2160, 17, false, 2).overMpsLimit ||
      !estimateVramNeed(false, 1920, 1080, 145, false, 2).overMpsLimit ||
      !estimateVramNeed(false, 1920, 1080, 145, false, 1).overMpsLimit ||
      estimateVramNeed(false, 1920, 1080, 145, false, 0).overMpsLimit) {
    std::printf("FAIL mps tensor limit\n");
    ++failures;
  }
  // Padding: 1080 -> 1088, 60 -> 65 frames, frames capped at the chunk length 145.
  const VramNeed p = estimateVramNeed(true, 1920, 1080, 300, false);
  if (p.modelH != 1088 || p.modelFrames != 145) {
    std::printf("FAIL padding %dx%d f=%d\n", p.modelW, p.modelH, p.modelFrames);
    ++failures;
  }
  // A 48 GB card next to Flame (~46.5 GB free, 5% reserve) must still take HD 145 fp8.
  if (estimateVramNeed(true, 1920, 1080, 145, false).gb > 46.5 - 2.3) {
    std::printf("FAIL HD 145 fp8 no longer fits a 48 GB card\n");
    ++failures;
  }
  std::printf(failures ? "%d failure(s)\n" : "test_vram_estimate: all checks passed\n", failures);
  return failures ? 1 : 0;
}
