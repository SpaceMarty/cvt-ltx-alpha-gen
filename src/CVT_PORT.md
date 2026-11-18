# CVT Resolve port: what changed from upstream

Upstream: <https://github.com/flamelogik/Flame_LTXAlphaGen_OFX>, commit `e9ab353`
(2026-10-27), MPL 2.0. This folder is that source with the changes below; the
modified files stay under MPL 2.0 (`LICENSE`), the vendored OpenFX under BSD.
Code changes are marked `CVT Resolve port` in comments. The upstream design
notes (moved to `upstream-notes/`) are kept as reference; where they talk
about Flame, Linux or NVIDIA, that part does not apply here.

## Mac only (until cvt1.50, which adds Windows: see "Windows" below)

- Apple Silicon only. Removed: the Linux platform layer
  (`platform_linux*.cpp`, NVML), `tools/nvml_probe.cpp`, the Linux symbol
  list, the remote, Linux, packaging and GitHub release scripts, the Flame
  installer (`packaging/`) and the Flame Python hook (`flame_hook/`, its test).
  CMake stops on anything but macOS. The CUDA branches inside the Python
  worker and Setup are left as they are: they never run on a Mac.
- Precision is hidden: MPS has no fp8, so bf16 (or Setup's fp16 copy) is the
  only choice.

## For Resolve

- Plugin id `com.creativevideotips.ltx.alphagen`, menu group `CVT`
  (OpenFX > CVT > LTX Alpha Gen), so it never clashes with the Flame build.
- New **Output** choice (`outputMode`; the alpha is always the matte):
  Source + Matte Alpha (default: the picture untouched), Matte (matte in RGB
  too), Overlay (the upstream magenta preview). The output clip asks for RGBA.
- New **Refresh** button: bumps the hidden refresh tick, so the host renders
  again and picks up finished mattes. The watcher's tick now always rises
  (it restarted at 1 per job, so a second job did not change it).
- Input Colour: hidden in Resolve (no OpenFX colour management there); new
  nodes start on Bypass. Elsewhere unchanged.
- Messages and hints name the host ("Resolve") instead of "Flame".
- System test: the node helper sets Output = Overlay for the upstream image
  checks; `process_chunks_merge_purge` also checks Source + Matte Alpha and
  Matte.

## Windows (cvt1.50)

Windows 10/11 x64 with an NVIDIA card (CUDA), beside the Mac build. The Mac code paths are
unchanged; what Windows needs is added beside them. Code changes are marked `Windows port`.

- **C++.** `ofx/src/win_compat.h/.cpp` supply the POSIX calls the shared sources use (`stat`,
  `unlink`, `mkdir`, `opendir`, `open`/`write`/`pread`, `localtime_r` ...) for UTF-8 paths, so
  those sources stay as they are (CMake sets `_CRT_DECLARE_NONSTDC_NAMES=0`, so the C runtime's
  own ANSI-code-page versions of those names are not declared). `oscompat.h` has the three calls
  that differ in form on both systems: `homeDirectory()` (HOME / %USERPROFILE%), `renameOver()`
  (std::rename / MoveFileEx with REPLACE_EXISTING, retried while a reader has the target open)
  and `fsPath()` (a wide path for file streams). `process_win.cpp` is `process.h` for Windows:
  CreateProcess with only the named handles inherited, a job object for the worker (kill = end
  the job), detached launches without a console window. `platform_win.cpp` is the platform
  layer: the temp folder for staging, NVML (`nvml.dll`, loaded at run time; the calls and structs
  of upstream's `platform_linux_nvml.cpp`) for GPU memory, Notepad for View Licence,
  ShellExecute for the web pages. `process.h` passes on the C runtime's own `process.h`, which
  `<thread>` includes and which this file's name hides on the include path.
- **Windows.** `ofx/ui/ltxag_ui.cpp` builds `ltxag_ui.exe` (beside the .ofx in `Contents/Win64`):
  the progress window with Cancel (zenity's stdin protocol, as `mac_progress.swift`), message
  boxes and Explorer's folder picker. Built with the plugin, not on the user's PC.
- **Python.** `oslayer.py` has the Windows forms of what job.py, the worker and Setup did with
  POSIX calls: `pid_alive` (on Windows `os.kill(pid, 0)` sends Ctrl+C, it does not ask),
  `kill_tree` (taskkill /T for killpg), `NEW_SESSION`, `lock_file` (msvcrt for flock), `replace`
  and `remove` (retry while another process has the file open), and `listen` / `connect` for
  the resident worker: Python has no Unix sockets on Windows, so there it is a loopback TCP port
  chosen by the system plus a random secret, both in the worker's info file, which every request
  must carry. Python is started with `-X utf8` on Windows (text files are UTF-8 on both systems).
- **CUDA again.** The worker's `Accel` has upstream's CUDA branches back (cvt1.17 took them out),
  and precision `fp8` (`QuantizationKind("fp8-cast")`). New: `--offload cpu|disk` and the
  manifest's `offload`, LTX-2's `OffloadMode`: the transformer's weights stay in system memory
  (or on disk) and go to the GPU a layer at a time. The plugin's Model Loading choice (Auto, On
  the GPU, In System Memory, From Disk; `planMemory` in plugin.cpp) decides it at Process; Auto
  takes the GPU when the estimate fits its free memory, else system memory when 26 GB (fp8) or
  48 GB (bf16) of it is free, else disk. A worker with another offload is restarted, like another
  precision. The Performance section shows on Windows (Precision, Model Loading). The estimate
  for streamed weights (`vram_estimate.h`) is built from upstream's Linux fits; three runs on an
  RTX 4090 came in 0.9 to 2.0 GB under it (listed there).
  The fixed decode budget (cvt1.32) and the fp16 copy stay Mac only.
- **Setup.** Windows has no system Python: `build.ps1` puts the pinned `uv.exe` beside the .ofx
  (checked against the manifest's new `uv.win32` SHA-256), and Download runs
  `uv run --managed-python --python <pinned> python setup_helper.py`, which fetches the pinned
  Python into `<Model Folder>/runtime`; the helper then uses that uv (`$LTXAG_UV`). No C compiler
  check (no Triton on Windows; LTX-2 falls back to its eager kernels), the CUDA smoke test
  again, the venv's `Scripts\python.exe`. `--link`: a symlink, else a hard link, else a copy.
  The lockfile's hash of the Windows `torchaudio 2.11.1+cu132` wheel is updated: the file at
  that URL was replaced after the lock was made (the same hash is now on PyTorch's release
  index), so `uv sync --frozen` refused it. The Linux hashes changed too; they are left alone.
- **Build and tests.** `build.ps1`, `install.ps1` (to `C:\Program Files\Common Files\OFX\Plugins`),
  `systest/run_systest.ps1` with `systest/fake_ui.cpp` as the fake window (put in a copy of the
  bundle), `test_os.h` for the unit tests, `worker/tests/test_resident.py` (the worker's server and
  a client with the model faked, on both systems). `.github/workflows/build.yml` builds and runs
  the unit tests and the fast system test on macOS and Windows runners.
- **Found with the real model** (and fixed): a venv's `python.exe` is a launcher on Windows, so the
  worker's own pid is not the one job.py started; job.py waited its full 120 s for the info file
  to name that pid before connecting (now an info file written since the start counts). And in
  the worker's one-shot mode (the smoke tools) the thread that reads stdin to notice its parent's
  end made the main thread hang at the next native library it loaded (`import scipy`), before
  any GPU work: `oslayer.wait_stdin_closed` asks the pipe instead of reading it on Windows.
- **Tested** on Windows 11, RTX 4090 24 GB, 94 GB RAM, Resolve Studio 21.2: unit tests, fast system
  test, a dummy-worker Process in Resolve; Setup complete (runtime, 3 files downloaded and 3 kept,
  all verified, prompt cache built in 85 s); the real model in fp8 with offload cpu: in Resolve
  120 HD frames in 6 chunks of 33 (690 s in the worker, 108-131 s a chunk, peak reserved
  11.1 GB) and 17 HD frames (78 s, 7.6 GB); `worker_smoke` 768x512x17 PASS, mean error 0.0058,
  5.5 GB. **Not tested:** bf16, offload none and disk, chunks over 33 frames, real footage, Fusion
  Studio, the Edit and Color pages.

## Changelog

- cvt1.00: first port (above).
- cvt1.01: Process no longer reads the clip twice. The ROI scan runs only if
  one of 5 sampled frames has a varying alpha (a Fusion MediaIn never does).
  The read shows the host's progress bar with Cancel (where the host has one)
  and logs the frame count and ms per frame. In Resolve, Process had blocked
  the UI (beach ball) while it read every frame twice.
- cvt1.02: Process reads only the comp's frame range (the effect's timeline
  bounds). In Fusion the Source clip reported a range far past the comp (the
  whole media), so Process read for minutes and then failed with "did not
  provide source frame". It also fetches the last frame first, so a range the
  host can't deliver fails at once, and logs the three ranges.
- cvt1.03: the range lookups are guarded one by one (Resolve threw for the
  output clip's frame range, so cvt1.02's Process ended at once with no
  message). Errors now also show in the Status field, and a failed Process
  says so there.
- cvt1.04: in Resolve the memory warning can't ask Yes/No and persistent
  messages are refused (that threw and ended Process), so a clip that may not
  fit now stops with a message in Status (scale down, ROI, smaller Chunk
  Size). The dummy worker skips the memory check unless Sim VRAM GB is set.
  Read-only fields (Status, ROI, Cache Size, Setup, Token, Version) are
  greyed-out text fields in Resolve, which drew label params with their name
  instead of their value. Output and Refresh follow the setup gate.
- cvt1.05: Fusion Studio is treated like Resolve (same OFX host: Status and
  the other read-only fields show their values, Input Colour hidden). A
  simpler Inspector: Alpha Gen open (Output, Process, Status, Refresh);
  Speed and Quality (Chunk Size, Merge, Frame Hold), Region of Interest and
  Cache folded; Licence, Models, Support and Version in one Setup group,
  folded once Setup is done; Show Marker, Precision, Unload GPU, VRAM Reserve
  and Save Default hidden with their defaults. The memory estimate has an
  M5-and-newer fit from two M5 Max runs (HD 33 frames 43.1 GB, HD 73 frames
  82.2 GB): the M1 fit was 18 GB high at 33 frames and 1.5 GB low at 73.
  Older chips keep the M1 fit. build.sh makes the release build by default
  (--dev for the Prototype test section); the system test uses build-dev/.
- cvt1.06: credits in the Setup section: Port by Chadwick, Creative Video Tips, a
  creativevideotips.com button (opens the site), Licence: Open source
  (MPL 2.0), Based on: LTX Alpha Gen for Flame by Jan Klier (Logik.tv). NOTICE
  names the port.
- cvt1.07: frames that differ in size work. Fusion trims each image to its
  Domain of Definition, so after a Crop, a Magic Mask or a garbage matte the
  frames are smaller than the format and change per frame; Process stopped
  with "Source frames differ in size within the clip". Process now builds one
  canvas, the union of every frame's region of definition (asked without
  rendering), places each frame on it (transparent black around it), and
  saves it (hidden canvasState) so render puts the matte back in place. Nodes
  processed earlier keep working (no canvas saved: the frame's own bounds).
- cvt1.08: the ROI section is "Input Alpha Hint", in this order: Model Crop
  (was ROI), Multiply by Input Alpha (was Multiply Result by ROI; now on for
  new nodes, no effect without an input alpha), Black Out Outside Alpha (was
  Black Out Outside ROI), Crop Margin (was ROI Margin). Param ids unchanged.
- cvt1.09: Inspector cleanup in Resolve and Fusion: HF Token is a one-line
  field (Fusion drew a tall box), the "Hugging Face token:" caption row (a
  Flame workaround) is hidden, no control can be keyframed (no diamonds; the
  node always processes the whole clip), and once Setup is complete View
  Licence, I Accept, HF Token, Download and Existing Models are hidden (they
  return if the licence or models stop being ready).
- cvt1.10: a Show Setup Options checkbox at the top of Setup (Resolve and
  Fusion, once Setup is complete) brings back View Licence, I Accept, HF
  Token, Download and Existing Models, to download the models again, use
  other model files or read the licence. Not saved with the comp.
  Also: the system test no longer pops up error dialogs on the user's screen
  (it leaves ~/.ltxag-no-dialogs in its own test HOME, and oslayer.message
  skips dialogs there), and the dummy's simulated out-of-memory says "GPU",
  not "CUDA". The CUDA branches left in the worker and Setup never run on a Mac.
  Post-Multiply Image (under Output, on for new nodes): with Source + Matte
  Alpha the picture is multiplied by the matte, as Fusion's MatteControl
  does, so a Merge takes it directly; the output clip's premultiplication
  state is reported to the host. Render-time only.
- cvt1.11: Purge Cache read the clip's whole media range, like Process before cvt1.03
  (minutes of beach ball in Resolve); it now uses the same comp range as
  Process (compRange) and also clears the saved canvas.
- cvt1.12: one version number. The plugin version hosts store with comps is
  the port's own (1.12; upstream's 1.0.0 is the CMake fallback only), and the
  Version field shows "cvt1.12". The major version stays 1, so saved comps
  keep loading. Leading zeros are dropped for OFX (1.08 -> 1.8).
- cvt1.13: the Version field shows just the number ("1.13").
- cvt1.14: Cache Limit (GB) in the Cache section, per user (cache_cap_gb,
  default 50). Lowering it trims the cache at once (least recently used
  first); Cache Size reads "0.6 of 50 GB".
- cvt1.15: the progress window estimates the time left from the start. Each
  real job records this Mac's speed (seconds per step per megapixel-frame,
  and the time before the first step and after the last, in steps) in
  ~/.config/ltx-alphagen/timing.json, blended 60 % new; the next job uses it
  until its own step 2. The dummy worker's speed is not used or saved.
- cvt1.16: the progress window's Cancel button never takes keyboard focus,
  so Space (play in Resolve and Fusion) can't press Cancel after the window
  was clicked.
- cvt1.17: the Python side is Mac only too. The worker's GPU class is MPS
  only (no CUDA branches, no CUDA allocator setting) and has no fp8 path;
  the OS layer has no Linux windows (zenity) or /proc; Setup has no CUDA smoke
  test or gcc check (the uv install command is unchanged: its Linux-only
  extra installs nothing on a Mac). The OS-layer test that opens a real
  window is opt-in (LTXAG_GUI_TESTS=1), so test runs show no window. Checked
  with the real model: worker_smoke 768x512x17 PASS (mean error 0.0054).
- cvt1.18: mattes are found by node, frame and settings, and a 64x36
  thumbnail of the source frame (<key>.thumb beside each matte) decides
  whether the picture is still the one processed: mean difference <= 0.02 and
  no cell over 0.25 (decoder noise passes; a +10 % gain or a moved subject
  does not). Upstream's exact pixel hash broke on long-GOP MP4s: Resolve
  decoded some frames slightly differently while Process read the clip than
  on later redraws, so those frames showed MATTE MISSING. Up to 4 slots per
  frame, so copies of a node (which share its id) on other clips keep their
  own mattes. Mattes made before cvt1.18 are still found by the old pixel
  key. Purge Cache, Purge All and the cleanup remove thumbnails with their
  mattes. A changed picture says so in Status.
- cvt1.19: the Merge choice "Smooth" is called "Dissolve" (same behaviour;
  setups store the index), and its hint says what each does. Resolve's Speed
  Warp can't be used for Flow Warp: plugins can't call Resolve's tools, the
  merge runs in the background job, and Fusion Studio has no Speed Warp.
- cvt1.20: a read-only Seams field in Speed and Quality lists where the last
  Process joined chunks, in comp frames ("1019-1033, 1037-1051", or "none (one
  chunk)"), saved with the comp and cleared by Purge Cache, so a seam can be
  checked, and checked again after changing Merge or Chunk Size.
- cvt1.21: the marker on frames that are still processing is much stronger (text
  100%, stripes 35%, was 30% and 8%) and says MAKING MAGIC instead of PROCESSING.
- cvt1.22: the marker has no diagonal stripes (they looked like a bug) and its letters
  are bolder (each dot a third wider and taller; same size and spacing).
- cvt1.23: Output = Overlay has a solid alpha, so the whole picture shows behind the
  magenta matte (the alpha used to be the matte, which cut the background away), and
  the marker is drawn in white over the picture there.
- cvt1.24: Fusion's Reset button starts the node fresh: this clip's mattes are removed
  and the node goes back to Not processed, as Purge Cache does (Reset used to restore
  only the visible settings, so the old mattes kept showing).
- cvt1.25: Refresh moved from under Process to the Cache section, with a hint saying
  it is rarely needed (the node redraws by itself when a job finishes).
- cvt1.26: Frame Hold is Off, 2x or 4x (param frameHoldMode). Auto and 8x are
  gone: Auto's plan (chosen for the 4x worst case, held at 12 px of motion) cost
  50% more on a mostly still clip (5 chunks for 63 frames), and its time could not be
  predicted; 8x was barely better than 4x. The old frameHold param stays defined,
  hidden and ignored, so saved comps load (at Off). The Auto code (motion.h,
  planAutoChunks and their unit tests) stays, unused; the auto_hold system test
  is removed. New Alpha Black Clip (0.005) and Alpha White Clip (0.995) under
  Post-Multiply Image: render time, before Multiply by Input Alpha, on real
  mattes only (clipMatte in marker.h); they clear the decoder's faint noise
  (0.001-0.005 in the background, 0.99-0.999 inside), stretching the values
  between so soft edges stay soft.
- cvt1.27: Skip Memory Check (Speed and Quality, off by default): Process runs
  even when the memory estimate says the clip won't fit, for Macs near the
  limit. A 64 GB M1 Max had 38.6 GB available in Resolve, 39.3 in Fusion
  Studio, against the M1 fit's 46.3 GB for a 420x432 12-frame clip (the 42 GB
  model is the floor). The job is logged as warned in vram_stats.csv, so its
  real peak can refit the M1 estimate. The Too large message names the option.
- cvt1.28: memory estimate refit for Apple Silicon (vram_estimate.h): one fit,
  max(40.8, 37.0 + 0.10 GB * Mpx * frames) + 1 GB, for the M5 and newer (bf16)
  and an M1-class Mac on the fp16 copy, from peak reserved MPS memory of 38 M5
  Max jobs and 3 M1 Max (64 GB) jobs. The cvt1.05 M5 fit was 30-40 GB high on
  long chunks (HD 73: 83.7 estimated, 50.2 used), so Auto fell back to 33-frame
  chunks in Resolve; the M1 fit refused every job on a 64 GB M1 Max (46.3 GB
  for 448x448, 33.7 used). The M1 Ultra bf16 fit stays for M1-class Macs without
  the fp16 copy. New Apple Silicon size limit: about 170 Mpx-frames per chunk
  (MPSGraph refuses tensors over INT_MAX elements; HD 145 failed after 9 s in
  the VAE encode, HD 81 and UHD 17 ran). Auto skips such chunk sizes; a fixed
  one stops in Status, even with Skip Memory Check.
- cvt1.29: Chunk Size is Auto, Half (73) or Quarter (33) (param chunkSizeMode);
  Full (145) is no longer offered, as HD is over the Apple Silicon size limit at
  145 frames. Auto still uses 145 when the picture is small enough. The old
  chunkSize param stays hidden and ignored (saved comps load at Auto). The job
  log has a line per seam: frames (comp numbers; Process sends first_frame), how
  much the two chunks' mattes differ (largest pixel difference, and the worst
  frame's mean), and with Flow Warp how many frames morph.py warped and the
  largest edge movement in pixels (morph() takes an optional stats dict).
- cvt1.30: Flow Warp acts only where it can help: the result is the dissolve,
  moved towards the warped matte inside a feathered mask of where the two
  chunks' mattes differ by more than 0.05 (grown by 2% of the long edge), and
  by sin(pi t), full in the middle of the seam and none at its ends. On a
  fast-running dog (HD, 3 Quarter chunks) Flow Warp was better than Dissolve
  mid-seam (frame 1032) but worse near the end (1039): the coarse flow (512 px)
  bent and resampled fur both mattes agreed on. The seam log line adds the
  share of the frame the warp changed. Dissolve stays the default.
- cvt1.31: Flow Warp is the default Merge, and both modes use the eased
  (smoothstep) seam curve. On the dog shot Flow Warp removed the ghosted back
  leg mid-seam (frames 1023, 1024, 1031, 1032) but lost near a seam end (1039):
  its linear curve kept ~12% of the other chunk there (Dissolve ~4%) and a
  ~38% warp; eased, that is ~4% and ~13%. Chunk 1 came out identical between
  two runs and chunk 3 differed (the upstream determinism note), so 1039 and
  1048 compare model runs as much as merges. The system test now checks
  Dissolve, Flow Warp being what the earlier runs use.
- cvt1.32: mattes repeat between runs more closely. Found by processing the same
  HD frames as identical chunks (LTXAG_HASH_STAGES=1 logs a fingerprint of each
  chunk's reference latent, decoder input and matte). The fix that matters for
  plugin jobs: the reference encode's dtype was the faster of bf16 and fp32 by a
  timed probe, a coin flip on an M5 Max (1.5-2.2 ms each) in every new worker,
  and in bf16 MPS gave different reference latents for the same HD frames anyway.
  Now always fp32 (the M1 had it already). A safeguard: the DiffVAE decode chose
  its tiles from the free memory (12 layouts between 20 and 68 GB at HD); now a
  fixed budget per Mac, half the Metal working set (53.8 GB on the M5 Max), or the
  real figure, logged, when less is free. Corrected in cvt1.33: this first said
  the tile budget was a main cause, from a test with the transformer resident
  (~70 GB free, on the 68 GB step). Plugin jobs rebuild the transformer per chunk
  and see ~105 GB free, so it would rarely have changed their tiles; the fixed
  budget does give them smaller tiles than before. Speed, one run each, 33 HD
  frames, M5 Max: 226.7 s bf16 with the large budget (as cvt1.31), 236.0 s fp32,
  240.3 s fp32 with the fixed budget: 6 % in all, kept for repeatability. What
  was measured: chunk 1 bit-identical between runs; two 3-chunk runs bit-identical
  only with the transformer resident and fingerprints on. Still open: chunks after
  the first sometimes come out in one of two versions (mean difference 0.0018,
  ~0.3 % of HD pixels, along edges), with the transformer resident or rebuilt, and
  with or without an emptied MPS cache; cause unknown, possibly MPS itself. Frame
  Hold's hint leads with what it is for: removing ghosting on fast-moving objects.
- cvt1.33: Process is ignored while the node's job is starting or running (a
  double click ran two jobs; the second to reach the worker unloaded it, and the
  other failed with "the worker process ended unexpectedly"). A job waiting for
  a worker that then unloads (Process on a second node) now starts a new worker
  instead of failing. In Fusion and Resolve the viewer shows the MATTE MISSING,
  PICTURE CHANGED, FAILED or CANCELLED marker: a failed render's picture was thrown
  away (red node, frozen viewer), and persistent messages are refused. Final
  renders still fail, and Fusion stops at that frame ("Render did not complete",
  tested by Chadwick). beginSequenceRender's isInteractive tells them apart;
  render()'s interactive flag is 0 for both. The Input Alpha Hint section is now
  Guided ROI (only its label; param names are unchanged). The Installer has a Read
  Me screen with the build's date. First 145-frame chunks on a Mac: a 512x576
  crop, 42.3 GB estimated, 40.8 GB peak.
- cvt1.34: tried to stop a final render as it begins if a frame of its range has
  no matte. Tested by Chadwick in Fusion Studio: it doesn't work. The check found
  the frame (1015) and failed the action, and Fusion carried on: it rendered 1005
  to 1020, saving 1015-1020, which the viewer had already drawn, with the marker
  (so the cache explanation is confirmed), and stopped at 1021, the first frame it
  had to draw. The failed action does nothing, but the log line naming the first
  missing frame is useful. Viewer or final now goes by isInteractive alone.
- cvt1.35: First Frame and Last Frame in Speed and Quality, Resolve only (hidden and
  ignored in Fusion Studio). Resolve's Fusion page doesn't pass on the render range
  (bars at 1043-1053, plugin told 1001-1101), and a plugin has no other way to read
  it. Fusion Studio does pass it on. Process only: Purge Cache still takes the whole
  comp. 0 leaves that end of the range as it is.
- cvt1.36: First Frame and Last Frame labels say "(0 = start)" and "(0 = end)", and the
  tooltip says 0 on both is the whole clip, the comp need not start at 0, and the numbers
  are the page's own (Chadwick: 0 and 0 could read as "nothing" or as "frame zero"). The
  effects category is Creative Video Tips (was CVT). No more persistent-message calls in
  Fusion and Resolve, which logged a warning on every frame. The watcher's status change
  is in an edit block in Resolve, trying to make the Edit and Color pages redraw after a
  job; whether it helps is untested. Tested on cvt1.35: Edit page, frames from 0 (edit
  frames, a 5x clip's 11 frames = every 5th source frame), First/Last worked. Color page
  the same; a second draw, probably thumbnails, logs PICTURE CHANGED and fails harmlessly,
  graying the node thumbnail; a Deliver there completes.
- cvt1.37: the effect has the CVT logo, white, 128x128 (ofx/resources/<plugin id>.png, the
  OFX bundle's icon, as Neat Video has). Process and Status are first in the Alpha Gen
  section, as the rest of it acts after the matte exists. The cvt1.36 edit block is gone:
  tested on the Edit page, it did nothing (Status and viewer still wait for the next
  interaction). Tested on cvt1.36: on a fast walker (shot 310, 16 frames), Frame Hold 2x
  gave a much more solid torso than Off (Chadwick), though still not fully solid.
- cvt1.38: on Resolve's Edit and Color pages a frame with no matte draws the marker
  (MATTE MISSING, PICTURE CHANGED ...) instead of failing. Tested on cvt1.37: on the Edit
  page a failed frame showed the plain picture, and a Deliver reported Complete with plain
  footage and no warning. Those pages give the output clip a frame range, the Fusion page
  and Fusion Studio don't, which tells them apart. On the Fusion page a final render still
  stops. Tested: Guided ROI on the Fusion page (ellipse alpha, crop 1216x1080, 83 s against
  129 s for 16 frames); shot 20's seams show no jump larger than normal frame changes
  (largest 0.0119 in a seam, 0.0125 outside), with a faint gray edge in the first seam
  that Chadwick judged good enough.
- cvt1.39: Setup has LTX-2.5 Model Page and Alpha Gen LoRA Page buttons that open the two
  Hugging Face pages in the browser, placed above the token and hidden with the other
  download controls once Setup is complete. The token field is now labelled Hugging Face
  Token, not HF Token (Chadwick: nobody will know what HF is).
- cvt1.40: Setup also has a Token Page button that opens the Hugging Face access tokens page,
  just above the token field.
- cvt1.41: the disk space notes and the Model Folder tooltip now allow about 125 GB on M2 Macs
  as well as M1. Measured: the Model Folder on an M2 Max is 109 GiB (about 117 GB), the same as
  an M1 Max, against 70 GiB on an M5 Max, so Setup made the fp16 copy there; the old note
  expected no copy on M2 to M4. M3 and M4 are not measured.
- cvt1.42: Download checks for Apple's Command Line Tools first and, if they are missing, says in
  Status what to run and keeps the token. Before, the helper's own check could never report it,
  because the system python3 that runs the helper comes from those tools. It looks on disk, since
  running that python3 or xcrun can open Apple's install window. Tested only by the system test's
  simulated failure, not on a Mac without the tools. Installer Read Me: new tagline, the models are
  no longer called open-source, the tools and the tested hosts are listed.
- cvt1.43: security review before publishing. Release builds ignore the Worker Python setting, which
  is saved with a comp, so a comp from someone else can't choose the program that Process or Download
  starts. Setup and jobs run with umask 022, and the settings, thumbnails and job manifests the plugin writes inside Resolve get fixed modes: Resolve's umask
  is 000, so the Python runtime, the settings and the cache came out writable by anyone on the Mac.
  The Hugging Face token no longer follows a redirect to another host. The node's logo no longer
  carries file metadata (EXIF and XMP held the Mac's user name).
- cvt1.44: the installer package no longer contains compiled Python caches, which carried the path
  of the Mac that built it. No change to the plugin itself.
- cvt1.45: the installer refuses to install on a Mac older than macOS 15 (the plugin is built for macOS 15,
  and macOS won't load it on an older system, so before this it installed and then simply didn't appear),
  and the README, guide and installer Read Me now say macOS 15 or later. No change to the plugin itself.

- cvt1.46: the Model Folder now starts at `~/LTX-2.5`, one folder for every LTX-2.5 plugin, and
  a new user who set up LTX Inpaint Still first keeps that folder. Anyone already set up keeps theirs. Setup's speed test
  for the extra 42 GB copy now needs 1.3x, not 1.2x, and doesn't decide again once it has decided, because an M5 Max sits right
  at 1.2x and could get the copy on one run and not the next.
- cvt1.47: new Link Existing Models checkbox. Fusion never showed the question that used to ask, so Download always copied.
  The token's length is no longer logged.
- cvt1.48: the folder fields have Choose buttons that open the Mac's Finder folder picker, in place of Fusion's browse
  control, which only picks files.
- cvt1.49: a Model Folder that was set up and has since been deleted is no longer offered from the saved settings: the field goes
  back to `~/LTX-2.5`. A folder on a drive that isn't mounted stays, since the drive may come back.
- cvt1.50: Windows (x64, NVIDIA): see "Windows" above. New Model Loading choice (hidden on a Mac). Also for the Mac: a Process
  with no cache folder yet (deleted, or a new Cache Folder typed in) makes it, where it used to end in MATTE MISSING because
  the thumbnails and pending markers could not be written.
- cvt1.50: the transformer is loaded without its audio branch. LTX-2.5 makes a sound track beside the picture
  (audio blocks and audio-video cross-attention, about 12 GB of the 42 GB model), and the matte never used it.
  The worker now builds upstream's video-only model, never reads the audio weights from disk, and gives each
  chunk no audio latent (injection 8). `LTXAG_KEEP_AUDIO=1` keeps the full model for A/B tests. The memory
  estimate is unchanged until jobs on this version are measured.
- cvt1.51: the memory check now matches cvt1.50, which no longer loads the audio part of the model. Every Apple
  Silicon estimate is 11.3 GB lower, the smallest saving measured on the M5 Max (an HD 73-frame chunk peaked at
  38.9 GB, down from 50.2 GB; it is now estimated at 42.0 GB, was 53.3). A 64 GB Mac, which gives the GPU about
  52 GB, can now take HD shots in 73-frame chunks. Not measured on a 64 GB Mac or an M1 yet.
- cvt1.52: corrects cvt1.51, which under-estimated one chunk. Its first HD 73-frame chunk peaked at 43.1 GB against an
  estimate of 41.9 GB (the others took 38.3-39.1 GB). Every Apple Silicon estimate is now 7.0 GB lower than in cvt1.49
  (not 11.3 GB), the smallest saving measured on any chunk, and never below 44.1 GB, the highest peak measured without
  audio (43.1 GB) plus the margin, which looks like the model load. An HD 73-frame chunk is estimated at 46.3 GB, which still
  fits the roughly 52 GB a 64 GB Mac gives the GPU. cvt1.51 was never committed.
- cvt1.53: new Weights menu in Speed and Quality: Auto (default), 16-bit, 8-bit. 8-bit holds the transformer's
  big layers (attention and feed-forward in every block) in 8 bits, one scale per row, converted while the
  16-bit file loads, with the alpha-gen LoRA merged in first (`worker/ltxag_worker/int8.py`, worker precision
  `int8`). In the test tool on the M5 Max it needed 12 GB less (HD 33 frames: 30.9 vs 42.9 GB), was about 2%
  slower at HD, and its mattes were within 0.06% of 16-bit on average. Auto uses 16-bit when the clip fits at any chunk size
  Chunk Size allows, else 8-bit; 8-bit mattes have their own cache entries (Auto shares the 16-bit ones). The
  memory estimate is 11.5 GB lower for 8-bit. On an M1 it runs with bf16 activations (no fp16), slower there;
  not tested on an M1 or a smaller Mac. Comps saved before load on Auto, which is 16-bit on any Mac where they ran.
- cvt1.54: the 8-bit memory estimate is 8.5 GB below 16-bit, not 11.5 GB. cvt1.53's first 8-bit job in Resolve (HD,
  65 frames in one chunk, 498 s) peaked at 34.3 GB against an estimate of 33.1 GB: in Resolve 8-bit saves about 9 GB,
  not the 12 GB the test tool measured. HD 73-frame chunks are now estimated at 37.8 GB in 8-bit (46.3 in 16-bit).
  The hints say about 8 GB less. cvt1.53 was never committed.
- cvt1.55: the reference frames are encoded in overlapping 640 px tiles (upstream's tiled encode, 128 px blended,
  still fp32). In one piece the encode was the job's peak: at HD 65 frames it took 24.4 GB and left the GPU cache at
  38.6 GB, the same in 16-bit and 8-bit, so 8-bit saved nothing at the peak. Tiled it takes 7.5 GB. Two runs give
  identical mattes; against the one-piece encode 0.12% of pixels differ by more than 0.1, and they match the test
  shape as well. The MPS cache is also handed back before each decode, and the job log shows each chunk's peak per
  phase (encode, load and step 1, steps, decode). The memory estimate for the M5 and newer is refit from eight runs
  (peak reserved): 16-bit HD 73 frames 36.4 GB (was 43.1), estimated 38.9; 8-bit HD 73 frames 33.6 GB, estimated
  34.9; 8-bit HD 33 frames 19.2 GB, estimated 23.4. `LTXAG_ENCODE_TILE=0` encodes in one piece, for tests only.
  cvt1.54 was never committed.
- cvt1.56: 16-bit runs the full model again (with its audio branch, as cvt1.49). On a motion-blurred HD shot the
  video-only model (cvt1.50-1.55) drew about half as many partly transparent edge pixels: harder edges on the arm,
  paddle and water where the picture is soft. Rendered four ways on the same 61 frames, the full model with the
  one-piece encode matched the cvt1.49 matte (0.08% of pixels off by more than 0.1); the tiled encode only gives a
  slightly different sample at the same softness, so it stays. Auto now tries the full model, then 16-bit without the
  audio branch (precision "bf16-video"/"fp16-video", the old 1.55 path, about 13 GB less), then 8-bit (still
  video-only). The explicit 16-bit choice is the full model. New M5 fit for it, from peak reserved memory: 768x432 17f
  40.6, HD 33f 42.7, HD 65f 48.4, HD 73f 49.9 GB; need = max(40.6, 36.8 + 0.086 x Mpx x frames) + 1 GB. So a 64 GB
  Mac (about 41 GB free) gets the video-only 16-bit at HD, which keeps the M1's speed: 8-bit on an M1 Max took 2595 s
  against 1834 s for the same 33-frame HD chunk. `LTXAG_VIDEO_ONLY=1` forces video-only 16-bit, for A/B tests
  (replaces `LTXAG_KEEP_AUDIO`). Cached 16-bit mattes made by cvt1.50-1.55 keep their key: clear them to remake.
- cvt1.57: the full model (16-bit) encodes its reference frames in one piece again, as cvt1.49, so its mattes match
  the public release; the tiled encode only gave a different sample (on a curly-hair shot 1.3% of pixels moved by
  more than 0.1, the same softness). It saved nothing there: the full model's peak is its load (768x432 17f 40.4,
  HD 33f 42.7, HD 73f 49.9 GB in one piece, the same as tiled). 16-bit video-only and 8-bit still tile, where the
  encode was the peak. And Auto (Weights and Chunk Size) now plans with this Mac's size, not the free memory at
  Process: the GPU working set less 5% and 8 GB for the host and system. A 64 GB M1 Max chose the full model with
  46.6 GB free and video-only with 42.1, so one shot came out two ways; it now always gets 16-bit video-only (plan
  41.2 GB: HD 73-frame chunks fit at 38.9), and an M5 Max with 128 GB always the full model. The memory check still
  compares the job with what is free now, and its message says to close other apps. M1 Max, full model, HD 32
  frames on cvt1.56: 43.0 GB peak against 43.7 estimated, 1999 s (video-only 1834 s, 8-bit 2595 s).
- cvt1.58: each chunk's log line now carries a fingerprint of the frames the host staged, before any colour
  conversion: a hash of the chunk and one per frame (6 hex digits), the mean RGB and range, and whether the values
  sit on 8-, 10- or 12-bit levels. The model gives the same matte for the same frames (a node re-rendered on
  2026-11-17 matched its 2026-11-07 matte to 0.001% of pixels; started at frame 1006 or 1029, 0.000%), yet two
  LTX nodes with identical settings fed by the same Loader in one Fusion comp disagree by 1.8% of pixels, every
  time. Re-rendering both and comparing the fingerprints shows which frames differ. About 0.3 s per HD 30-frame
  chunk. No change to the mattes.
- cvt1.59: new Output choice "Decontaminated + Matte Alpha" (added last, so saved setups keep theirs) and
  "Decontaminate Erode (px)" (default 64). Contaminated edge pixels, where the background shows through hair,
  motion blur and soft edges, get the subject's own colour: Blur-Fusion foreground estimation (Forte and Pitie,
  ICIP 2021; radii 90 then 6 px at 1080p), clamped to the solid core's local mean +- 1.5 std, blended towards the
  core colour pushed outwards (push-pull pyramid) where the matte is thin. Only pixels with matte between 0.02
  and 0.98 and within the erode width of background change; the matte and the background stay as they are.
  Render-time (ofx/src/decontam.cpp, own code; ideas from a GPL ComfyUI node, no code copied), in the colour space
  the host delivers, about 0.25 s per HD frame. Post-Multiply Image applies to it as to Source.
- cvt1.60: Decontaminated + Matte Alpha is now a strict edge extend: the solid core (matte >= 0.98), eroded by
  "Alpha Erode" (renamed from "Decontaminate Erode"; now the erode of the core, default 2 px, param id alphaErode),
  has its colour pushed outwards and that replaces every other subject pixel. 1.59's Blur-Fusion estimate kept
  per-pixel source detail, so background lettering showed in the soft edge of an out-of-focus face (sh010), and
  filled matte that reached past the visible edge with bright skin (a cut-out outline). Faint matte haze more
  than 256 px (at 1080p) from the subject keeps the source, as does pure background. Thin hair strands now carry
  the hair's smooth colour, not their own highlights. About 0.05 s per HD frame (was 0.25 s).
- cvt1.61: "Edge Blend" (px, default 4, param id edgeBlend) under Alpha Erode: in Decontaminated + Matte Alpha,
  the original picture fades into the extended edge colour over that many pixels, going in from where Alpha
  Erode stops. The fade stays inside the solid part of the matte (>= 0.98), so no background detail returns;
  the colour is now taken from beyond the blend. 0 is a hard switch (cvt1.60 had a fixed 1.5 px step).
- cvt1.62: Edge Blend is shown as "Edge Fade" (same param id edgeBlend, so 1.61 setups keep their value), and new
  "Edge Blur" (px, default 0 = off, param id edgeBlur): a blur of the decontaminated picture, the last step before
  the matte, about a Gaussian of that size (three box passes). It averages only the subject's pixels (matte >= 0.02
  near the subject), so the old background next to the edge never comes back, and it applies where the extended
  colour does, tapering off with the fade so the picture further in stays sharp. About 0.14 s per HD frame at 4 px.
- cvt1.63: "Guide With Alpha" (Guided ROI group, param id roiGuide; on for new nodes, off in setups saved before,
  needs Process). The input alpha (Magic Mask, roto or a rough shape) is thresholded at 0.5, grown 20 px and
  feathered (about a Gaussian of 10 px), both scaled with the frame height from 1080, and staged as a 4th channel
  (frame file channels 4). The worker passes it as the IC-LoRA guide's conditioning_attention_mask (already in
  ltx_pipelines; the ComfyUI Comfy Pro node's "v13" uses the same with SAM3), so LTX mattes that subject only and
  still draws its edges. Tested on sh090 (33 HD frames): a mask on one woman cut the matte outside her by 99% with
  hair as good; a rough moving box on a man LTX had ignored gave a clean full-body matte. Guided mattes get their
  own cache key (ROI k bit 1). Also: test_roi now fails on a failed check.
- cvt1.64: "Guide Grow (px)" (param id roiGuideGrow, default 20 at HD, 0-100, needs Process; non-default values
  get their own cache key) and an optional "Occlusion Mask" input (clip "Occlusion", a mask clip in the Filter context
  as openfx-misc's Mask inputs; not on Resolve's Color page). With Guide With Alpha, the occlusion mask is taken out of the
  guide at Process (ROI k bit 2), and at render it cuts the matte where its own mask is solid (smooth 0.5-0.98),
  so the soft edge between the subject and what is in front stays LTX's. For a subject passing behind someone (sh090:
  guided by the cart man's Magic Mask, LTX also matted a strip of the old man in front). "Black Out Outside Alpha"
  is hidden (Guide With Alpha does it better; still works in setups that have it on). Defaults halved: Alpha Erode
  1 (was 2), Edge Fade 2 (was 4).
- cvt1.65: "Crop Scale" (Off / 2x / 3x, Speed and Quality, param id cropScale, needs Process): with an input alpha,
  the crop is scaled up by that whole factor (Catmull-Rom, the guide channel too) before the model sees it, and
  job.py averages each matte back down over k x k blocks before pasting it into the frame. Memory, chunk size,
  the 6K check and the job size use the scaled size; a crop that would end up bigger than about HD (1920x1088
  pixels) is scaled less, logged. The scale is in the ROI state (bits 3-4) and so in the cache key. Tested
  outside the node on sh470 (two people about 270 px tall, a 640x360 crop at 3x): the people separate, a
  ponytail and hands survive, the curly hair is tighter, same time as an HD job. Labels renamed: Output
  "Pixel Spread + Matte Alpha" (was Decontaminated + Matte Alpha), "Pixel Spread Erode / Fade / Blur" (were
  Alpha Erode, Edge Fade, Edge Blur); param ids unchanged.
