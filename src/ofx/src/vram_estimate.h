// Size-aware peak-VRAM estimate for one worker job (decisions 12/34, build step 3 data).
// Fitted to measurements on the reference machine (RTX PRO 6000 Blackwell 96 GB, 2026-10-10/11):
//   official CLI, HD 1920x1088:  fp8 97f 34.1 / 145f 41.3 GB;  bf16 49f 44.9 / 145f 58.8 GB
//   official CLI, UHD 3840x2176 49f: fp8 54.1 / bf16 67.8 GB
//   our worker, HD 65f: fp8 28.3 GB rebuilt per chunk, 37.7 GB with the transformer resident
// need = base(precision) + 0.0718 GB * Mpx * frames + 0.70 GB * max(0, Mpx - 2.09) + 1 GB,
// where Mpx and frames are the PADDED model sizes of the largest chunk (/32, 8n+1, <= 145).
// bf16 base = fp8 base + 17.3 GB (the transformer weight difference).
// Resident transformer (Unload off; refit 2026-10-11 from Flame test 2 + an A/B run): once the
// transformer exists (every chunk after the first, every chunk of a reused worker) the reference
// encode at the start of a chunk runs on top of it, so
//   need = max(need above, T + 1.56 GB + 0.1088 GB * Mpx * frames + 0.7 GB context + 1 GB),
// T = the held transformer, reserved: fp8 21.7 GB, bf16 36.8 GB. Fitted on reserved memory
// (what free memory loses): HD 145 fp8 56.2 GB, 720p 57 bf16 44.2 GB. Pure, no OFX.
// Apple Silicon (mps = true, decision 65; bf16 only, MPS has no fp8): measured on an M1 Ultra
// (96 GB Metal working set, torch 2.14.1, our worker, 2026-10-12) as the lowest free working set
// (what the process held, incl. the MPS allocator's cache): 720p 49f 52.3, HD 33f 60.4,
// HD 73f 79.7 GB. need = 44.5 + 0.231 GB * Mpx * frames + 1 GB; HD 145 -> ~115 GB, over the
// working set. The resident-transformer case is not measured on MPS (UNVERIFIED): same formula.
// CVT port (cvt1.05): M5 and newer (mps = 2) have their own fit, from peak reserved MPS memory on
// an M5 Max (107.5 GB working set, torch 2.14.1, bf16, 2026-11-03): HD 33f 43.1 GB (Resolve job),
// HD 73f 82.2 GB (worker_smoke). Memory grows about twice as fast with frames as on the M1, so
// the M1 fit was 18 GB high at 33 frames and 1.5 GB low at 73:
//   need = 11.0 + 0.47 GB * Mpx * frames + 1 GB; HD 145 -> ~155 GB (never fits), UHD 33 -> ~142.
// Only two points: refit when more runs are recorded (logs/vram_stats.csv).
// CVT port (cvt1.28): that M5 fit was 30-40 GB high on long chunks, and the M1 Ultra fit is bf16;
// an M1-class Mac on the fp16 copy (decision 68) needs the same as the M5 in bf16. One fit for
// both (mps 2 and 3), from peak reserved MPS memory, which the MPS cache keeps at ~40.5 GB for
// small jobs (used = free at start - lowest free is lower, or noisy with other apps):
//   M5 Max (107.5 GB working set, Resolve and Fusion Studio, 38 jobs, 2026-11-03/04): HD 9f 40.5,
//   17f 40.7, 25f 41.6, 33f 42.8, 41f 44.7, 49f 45.6, 65f 48.6, 73f 50.2, 81f 51.4, UHD 17f 52.1;
//   M1 Max (64 GB, 51.8 GB working set, fp16, Fusion Studio, 2026-11-04): 448x448 17f 40.6,
//   HD 17f 40.8, HD 33f 43.0.
//   need = max(40.8, 37.0 + 0.10 GB * Mpx * frames) + 1 GB; HD 33 -> 44.9, HD 73 -> 53.3,
//   HD 145 -> 68.3 (not measured), UHD 17 -> 52.2.
// Windows port (streamed = true): the transformer's weights are not on the GPU but sent to it a
// layer at a time (LTX-2's OffloadMode cpu / disk; its documentation gives about 5 GB of GPU
// memory for that buffer). What must fit is then the larger of the denoising (buffer + the
// per-frame term above) and the reference encode, which the resident-transformer fit above
// measured apart from the transformer (1.56 GB + 0.1088 GB * Mpx * frames + 0.7 GB context):
//   need = max(6.0 + 0.0718 * Mpx * frames, 2.26 + 0.1088 * Mpx * frames)
//          + 0.70 GB * max(0, Mpx - 2.09) + 1 GB; HD 33 -> 12.0, HD 73 -> 19.9, HD 145 -> 36.2.
// The same for bf16 and fp8: the weights' size then counts against system memory, not the GPU.
// Built from those two fits. First streamed runs (RTX 4090 24 GB, Windows 11, torch 2.13.0+cu132,
// fp8, offload cpu, 2026-11-11), peak reserved against this estimate: HD 33f 11.1 GB (11.9),
// HD 17f 7.6 (9.6), 768x512 17f 5.5 (7.0). Cautious by 1-2 GB; bf16, offload disk and longer
// chunks are not measured.
// CVT port (cvt1.51, corrected in cvt1.52): without the audio branch (cvt1.50) every Apple Silicon
// need above is 7.0 GB lower, the smallest drop measured (M5 Max, peak reserved, HD 73f: 50.2 ->
// 38.3-39.1 GB, one first chunk 43.1 GB; 768x432 17f 40.4 -> 27.4 GB):
//   need = max(43.1, 30.0 + 0.10 GB * Mpx * frames) + 1 GB; HD 33 -> 44.1, HD 73 -> 46.3,
//   UHD 17 -> 45.2. The floor is the highest peak measured without audio (HD 33f took 42.9 GB in
//   worker_smoke), so small clips are over-estimated (768x432 17f: 44.1 vs 27.4 GB). The M1 Ultra
//   fit drops by the same 7.0 GB (not measured on an M1).
#pragma once

namespace ltxag {

struct VramNeed {
  double gb;          // estimated peak
  int modelW, modelH;  // padded
  int modelFrames;     // padded, of the largest chunk
  bool overMpsLimit = false;  // cvt1.28: too big for MPS whatever the memory (see below)
};

// cvt1.28: on Apple Silicon a chunk can't be larger than about 170 Mpx-frames (the padded model
// size): MPSGraph refuses tensors with more than INT_MAX elements, and the video VAE's first
// layers make one about 8-13 x the picture. HD 81 frames (169 Mpx-frames) and UHD 17 (148) ran
// on the M5 Max; HD 145 (303) failed after 9 s in the VAE encode (2026-11-04), using 17 GB.
// So HD can't use Full (145) on a Mac. A memory problem it is not: Skip Memory Check can't help.
constexpr double kMpsMaxMpxFrames = 170.0;

// mps: 0 = CUDA, 1 (or true) = Apple Silicon with the M1 fit, 2 = the M5-and-newer fit,
// 3 = an M1-class Mac on the fp16 copy (cvt1.28; the same fit as 2).
// CVT port (cvt1.55): with the reference encoded in tiles, the M5-and-newer fit (mps 2, 3) is
//   16-bit: 26.5 + 0.075 GB * Mpx * frames + 1 GB; HD 33 -> 32.7, HD 73 -> 38.9, HD 81 -> 40.2
//   8-bit:  13.0 + 0.137 GB * Mpx * frames + 1 GB; HD 33 -> 23.4, HD 73 -> 34.9
// from peak reserved memory on an M5 Max (vram_estimate.cpp); not measured on a smaller Mac or an M1.
// cvt1.56: on Apple Silicon 16-bit is the full model again (audio branch included, as cvt1.49), whose
// edges stay softer on soft pictures; the video-only model (cvt1.50-1.55) is Auto's middle step:
//   16-bit full:       max(40.6, 36.8 + 0.086 GB * Mpx * frames) + 1 GB; HD 33 -> 43.7, HD 73 -> 50.9
//   16-bit video-only: the cvt1.55 16-bit fit above
// Weights (Apple Silicon only): Full16 the whole transformer in 16 bits; VideoOnly16 without its audio
// branch (cvt1.50); Int8 (cvt1.53) 8-bit weights, always video-only.
enum class Weights { Full16 = 0, VideoOnly16 = 1, Int8 = 2 };
VramNeed estimateVramNeed(bool fp8, int width, int height, int framesInLargestChunk,
                          bool residentTransformer, int mps = 0, Weights weights = Weights::Full16,
                          bool streamed = false);  // Windows port: streamed

}  // namespace ltxag
