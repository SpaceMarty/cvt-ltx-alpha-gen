# LTX Alpha Gen for DaVinci Resolve

**Local, stable AI roto for DaVinci Resolve. Free, on your Mac.** An OFX plugin
for DaVinci Resolve and Fusion Studio that makes an alpha matte for a clip with
Lightricks' LTX-2.5 Alpha Gen model.

From [Creative Video Tips](https://creativevideotips.com), ported by Chadwick
from [LTX Alpha Gen OFX for Flame](https://github.com/flamelogik/Flame_LTXAlphaGen_OFX)
by Jan Klier, maintained by [Logik.tv](https://logik.tv). This is an independent
port, not made or endorsed by Lightricks or Blackmagic Design.

## Download

### [Download the installer: LTXAlphaGen-1.65.pkg](https://github.com/creativevideotips/cvt-ltx-alpha-gen/releases/download/v1.65/LTXAlphaGen-1.65.pkg)

### [Download the guide (PDF)](https://github.com/creativevideotips/cvt-ltx-alpha-gen/releases/download/v1.65/LTX_Alpha_Gen_Guide.pdf)

These links download the files straight away. You won't find the installer in the list of code files above: GitHub keeps it on the [Releases page](https://github.com/creativevideotips/cvt-ltx-alpha-gen/releases/latest), under **Assets**. The installer is only the plugin, and you also need to [download the models](#setup-download-the-models).

![The first, middle and last matte of a shot of a circular saw and an arm](fusion_studio_first_mattes.png)


## Is it for you?

- **Try Magic Mask first.** Resolve's Magic Mask is far faster, so use it first.
  When its matte isn't good enough, use LTX Alpha Gen for the more precise work.
- **Mac with Apple Silicon, or a Windows PC with an NVIDIA card.** The Mac version is
  the released one, and what this page describes. The Windows version is new and has no
  installer yet: see [Windows](#windows-nvidia).
  There is no Linux version.
- **macOS 15 (Sequoia) or later.** The plugin is built for macOS 15, and the installer
  will not install on an older macOS.
- **Tested with Resolve Studio and Fusion Studio.** It has not yet been tested in the
  free version of Resolve. Magic Mask is Studio-only, but any roto or garbage matte
  works for [Guided ROI](#choosing-the-subject-guided-roi).
- **64 GB of RAM or more.** With 128 GB it runs the full model. On a 64 GB Mac it
  uses a lighter version of the model (16-bit without its sound part) and still runs
  HD in 73-frame chunks ([Weights](#the-controls)). Tested on an M5 Max with 128 GB and an
  M1 Max with 64 GB. Macs with less memory haven't been tested: 8-bit weights need
  about 20-23 GB for 33 HD frames, so they would need a smaller picture or a Guided
  ROI crop.
- **It takes time.** Expect several minutes for a short HD clip on a fast Mac
  ([measured speeds](#size-and-speed)). The GPU stays busy, so playback is slow
  while a job runs.
- **It needs disk space.** About 80 GB for the models, or about 125 GB on some
  Macs ([which Macs](#disk-space)).
- **It needs Apple's Command Line Tools,** a free download from Apple
  ([Setup](#setup-download-the-models) covers it). You don't need to install Python:
  Setup brings its own.

## Install

1. [Download the installer](#download) from the top of this page.
2. Quit Resolve and Fusion Studio, then double-click the pkg. It is signed and
   notarized, asks for your admin password, and installs into
   `/Library/OFX/Plugins`.
3. Start Resolve. The node is in the Fusion page under Effects > OpenFX >
   Creative Video Tips > LTX Alpha Gen.

## Setup: download the models

The installer is only the plugin. The models are a separate download from
Hugging Face, and Lightricks asks each person to accept their terms, so you need
a free Hugging Face account.

First, install Apple's Command Line Tools if you don't have them. In Terminal, run
this and follow the window it opens. If it says they are already installed, carry on.

```bash
xcode-select --install
```

If you skip this, Download stops and Status tells you to run it.

The rest happens in the node's **Setup** section.

1. Press **View Licence**, then tick **I Accept**.
2. Press **LTX-2.5 Model Page** and **Alpha Gen LoRA Page**. Each one opens its
   page in your browser. Log in or sign up, and accept the terms in the box near
   the top. Access is granted at once. Do both pages: if you miss one, Download
   stops with "Hugging Face refused the download (HTTP 403)".
   - https://huggingface.co/Lightricks/LTX-2.5
   - https://huggingface.co/Lightricks/LTX-2.5-22b-IC-LoRA-Alpha-Gen
3. Press **Token Page**, or go to https://huggingface.co/settings/tokens. Choose
   **Create new token**, type **Read**. The name is only a label for you. Copy the
   token (it starts with `hf_`).
4. Paste it into **Hugging Face Token** and press Return.
5. Check the **Model Folder** (by default `~/LTX-2.5`). If you already
   have some of the model files, for example in a ComfyUI folder, set **Existing
   Models** to that folder: matching files are checked and copied instead of
   downloaded. Tick **Link Existing Models** to link to them instead of
   copying them, which saves up to 73 GB, as long as that folder stays available.
6. Press **Download**. A window shows the progress. It installs its own Python
   and everything the model needs into the Model Folder, separate from anything
   else on your Mac, then downloads the models and checks every file. Keep every file in the folder,
   including the `.verified` ones.

**About the token:** it is used for this download only and is never stored. The
field clears as soon as Download starts. Keep it private, because with it someone
can download anything your account can. Once the models are on every Mac, you can
revoke it on the same page. If you record your screen, blur it or use a throwaway
token.

Once Setup is complete, these controls hide themselves. Tick **Show Setup Options**
to bring them back.

### Disk space

Every Mac downloads the same files, about 75 GB. At the end of Setup the plugin
times the model's core maths in two number formats. If fp16 is at least 1.3x faster
than bf16 on your Mac, it also makes an fp16 copy of the 42 GB main model and uses
that, so older chips run faster.

| Mac | Extra copy? | Folder size | Free space to allow |
|---|---|---|---|
| M1 family | yes, about 2x faster (measured on an M1 Max) | about 117 GB | about 125 GB |
| M2 family | yes (folder size measured on an M2 Max) | about 117 GB | about 125 GB |
| M3, M4 families | not measured yet | about 75 or 117 GB | about 125 GB to be safe |
| M5 family | no, same speed (measured on an M5 Max) | about 75 GB | about 80 GB |

If the disk fills up during the copy, Setup skips it and the plugin still runs,
just more slowly.

## Using it

Work on the Fusion page or in Fusion Studio, in HD:

```
MediaIn1 (or Loader) → Resize 1920×1080 → LTX Alpha Gen → Bitmap (Channel: Alpha) → mask
```

1. Press **Process**. The host pauses for a couple of seconds while it hands over
   the frames, then the job runs on its own with a progress window and Cancel.
   Status follows it, and the viewer updates when it is done.
2. Use the matte. A **Bitmap** node (Channel: Alpha) turns it into a mask, and masks
   scale to whatever they are applied to, so it works on the full-resolution picture:
   - **Grade the subject:** the Bitmap into a ColorCorrector's effect mask on the
     original MediaIn.
   - **Cut it out:** a MatteControl with the original as Background and the Bitmap
     in Garbage Matte, with Invert on, then a Merge over a new background.

### Feed it Rec.709

The model expects normal Rec.709 video, and the plugin passes on the pixels as they
arrive. Log footage still gives a good matte, but in a side-by-side test on one shot,
the matte from a Rec.709 picture was better. So convert before the node, and use its
matte on your untouched footage: source > Color Space Transform (log to Rec.709) >
LTX Alpha Gen, then take the matte to the original branch.

## Choosing the subject: Guided ROI

There is no text prompt: the model picks the main subject of what it sees. To choose
the subject, give the picture coming in an alpha, such as a Magic Mask, roto or even
a rough shape. It does three things:

- **Which subject the model mattes.** With **Guide With Alpha** (on for new nodes), the
  alpha steers the model to that subject: other people and objects drop out, and the
  model still draws the edges, hair included. The alpha is grown and softened first
  (**Guide Grow**, 20 px at HD), so a rough mask is enough. In a test with four people
  in a street, a mask on one of them cut everything else from the matte, and a rough
  moving rectangle on a man the model had ignored gave a clean full-body matte.
- **Where the model looks.** The model only sees a crop around the area the alpha
  covers over the whole clip, plus a margin. That is much faster. A mask that sweeps
  across half the frame makes a big crop.
- **What you keep.** **Multiply by Input Alpha** (on by default) trims the matte to
  your mask, instantly.

The alpha has to come with the untouched picture: the plugin uses the picture coming
in as the model's input, so don't cut it out (premultiply) before the node.

```
Loader1 ─────────────────────── Background ┐
   └→ MagicMask1 ─────────────── Foreground ┴→ MatteControl1 → LTX Alpha Gen
```

1. Magic Mask the subject. A quick, rough mask is fine for the guide.
2. **MatteControl:** Background = the original picture, Foreground = the mask.
   Matte: Combine **Combine Alpha**, Combine Op **Copy**, **Post-Multiply Image off**.
   (Check with the viewer's colour channels: the whole picture should be there, not
   black around the subject.)
3. Press Process. **Model Crop** shows the crop size, for example "Crop 1280x1024".

With Multiply by Input Alpha on, a tight mask also cuts the hair at the mask's edge:
grow the mask a little (ErodeDilate), or turn Multiply off and let the guide do the
work.

**Something in front of the subject.** The **Occlusion Mask** input is for an object
closer to the camera than your subject, in front of it: a person walking past, an arm,
a pole. The guide reaches a little past your mask, so where that object overlaps the
subject, the model can matte a strip of it. Connect a mask of the object in front (a
second Magic Mask) to the Occlusion Mask input: it is taken out of the guide, and cut
from the matte where it is solid. The soft edge between the two stays the model's. The
input is on the node in Fusion Studio and on Resolve's Fusion page.

**Small or distant subjects.** **Crop Scale** (Speed and Quality) scales the crop up
2x or 3x for the model and the matte back down, for more detail on edges, hands and
gaps between people. As a rule of thumb at HD: 3x for a subject under about 300 px
tall, 2x up to about 500 px, off above that. It costs about as much as an HD job, and
a crop that would end up bigger than HD is scaled less.

A small subject in a UHD shot can be processed at full resolution with Guided ROI,
without a Resize, as long as the crop is no bigger than about an HD frame in area.

## The controls

**Alpha Gen**

| Control | What it does |
|---|---|
| Process | Makes mattes for every frame of the comp, or the render range in Fusion Studio. |
| Status | What the node is doing, or why it stopped. |
| Output | **Source + Matte Alpha** (default): the picture with the matte in its alpha. **Matte**: the matte in RGB and alpha. **Overlay**: the picture with the matte in magenta, to check it. **Pixel Spread + Matte Alpha**: like Source + Matte Alpha, but the contaminated edge pixels (where the old background shows through hair, motion blur and soft edges) get the subject's own colour, spread out from its solid part, so no colour or detail of the old background comes along into a new one. |
| Post-Multiply Image | On (default): the picture is multiplied by the matte, ready for a Merge. Off: the picture untouched. |
| Pixel Spread Erode | For Pixel Spread only: how many pixels the solid part of the matte is eroded before its colour is spread out (default 1). Raise it if the solid part's own edge still carries a rim of the old background. |
| Pixel Spread Fade | For Pixel Spread only: how many pixels the original picture takes to fade into the spread colour, further in (default 2). The fade stays inside the solid part, so it brings no background back. |
| Pixel Spread Blur | For Pixel Spread only: blurs the picture where the spread colour and the original meet (default 0, off), blending only the subject's own pixels. Useful on soft or motion-blurred subjects. |
| Alpha Black Clip | Matte values at or below it become 0 (default 0.005). This clears the faint haze the model leaves in the background. |
| Alpha White Clip | Matte values at or above it become 1 (default 0.995), so the inside of the subject is solid. Soft edges stay soft. |

None of these except Process needs reprocessing.

Pixel Spread has no detail of its own in soft edges: thin hair strands and motion-blurred
edges carry the subject's smooth colour, with their shape coming from the matte.

**Speed and Quality** (the defaults are fine)

| Control | What it does |
|---|---|
| First Frame, Last Frame | Resolve only: process just these frames, to try a short section first. 0 means from the start or to the end. Resolve's yellow render range bars don't limit Process; in Fusion Studio they do, so it doesn't have these. |
| Chunk Size | **Auto** (default; leave it there), **Half (73)** or **Quarter (33)** frames per model run. Auto picks the longest that fits your Mac's memory: up to 145 frames for small pictures, 73 at HD. Shorter chunks need less memory, but join at more seams. |
| Crop Scale | With an input alpha: **Off** (default), **2x** or **3x**. Scales the crop up for the model and the matte back down, for more edge detail on small or distant subjects ([Guided ROI](#choosing-the-subject-guided-roi)). A crop that would end up bigger than HD is scaled less. Needs Process. |
| Weights | How the model is held in memory. **Auto** (default; leave it there) uses **16-bit**, the reference quality, when the clip fits your Mac's memory. If it doesn't, Auto uses 16-bit without the model's sound part (about 13 GB less; edges can come out a little harder on motion-blurred or soft pictures), and if that doesn't fit either, **8-bit** (less than half the memory of 16-bit, close to it in quality, but about 40% slower on M1 Macs). Auto decides from your Mac's size, not from what happens to be free, so a shot comes out the same each time. 8-bit mattes are kept apart from 16-bit ones in the cache. |
| Merge | How chunks join where they overlap. **Flow Warp** (default) lines up moving edges with optical flow while dissolving, so they slide instead of doubling. **Dissolve** is a plain cross-dissolve; try it if Flow Warp ever smears an edge. |
| Seams | The frames where the last Process joined two chunks. Check moving edges there with Output on Matte. |
| Frame Hold | For fast motion (a running leg, a whip pan). **2x** or **4x** repeat each frame so the model sees less motion per step, for sharper edges and a more solid core, but processing takes about 2x or 4x as long. Leave it **Off** for most shots and try 2x on a final shot with fast motion. 4x is rarely worth it. |
| Skip Memory Check | Process even when the memory estimate says the clip won't fit. The estimate is cautious, so it may work. If not, macOS swaps to disk and the job slows down badly or fails, but nothing is damaged. |

**Guided ROI**

| Control | What it does |
|---|---|
| Model Crop | The crop the model saw at the last Process. |
| Multiply by Input Alpha | Trims the matte to the input alpha. Instant. On by default. |
| Guide With Alpha | The input alpha steers the model to that subject. On for new nodes. Needs Process. |
| Guide Grow | How far the guide reaches past your mask, in pixels at HD (default 20), so hair outside a tight mask is still matted. Lower it when the subject is next to someone else. Needs Process. |
| Occlusion Mask (input) | A mask of an object closer to the camera, in front of the subject: taken out of the guide and cut from the matte where it is solid. |
| Crop Margin | Extra pixels of context around the alpha's area (default 64). Raise it if the matte is cut off in a straight line at the crop edge. Needs Process. |

**Cache**

Mattes are kept on disk, about 4 MB per HD frame, and renders reuse them. A real change
before the node (a grade, the Resize, a trim, another clip) needs a new Process. **Purge
Cache** deletes this node's mattes and **Purge All** deletes everyone's. **Cache Limit**
(default 50 GB, about 12,000 HD frames) is the most the cache may grow to: the least
recently used mattes go first, and mattes unused for 30 days are deleted too.

**Refresh** is rarely needed: if the viewer still shows MAKING MAGIC after a job has
finished, it picks up the new mattes without reprocessing.

## Check your renders: frames without a matte

A frame that has no matte shows **MATTE MISSING** in the viewer. It shows **PICTURE
CHANGED** if the picture coming in has changed since Process.

**That text can end up in your render:**

- **Fusion page and Fusion Studio:** a render stops at the first frame with no matte
  that it has to draw. But frames you have already viewed in this session can be saved
  with the text in them, and a Deliver made up only of such frames reports Complete.
- **Edit and Color pages:** a Deliver always carries the text on those frames and
  reports Complete.

So process the whole range you are rendering, and check the result.

## Edit and Color pages

The plugin also works as an effect on these pages.

- **Adding it:** on the Edit page, drag it onto the clip, or select the clip and
  double-click the effect. On the Color page, drag it onto a node.
- **Frames:** these pages count edit frames from 0, while the Fusion page counts source
  frames, usually from 1001.
- **After a job:** if the viewer still says MAKING MAGIC or shows an old picture, move
  the playhead one frame.
- **While a job runs on the Color page:** the viewer shows the picture rather than
  MAKING MAGIC. The progress window and Status show that it is running.
- **Remote grades:** a node on the Color page goes into the clip's grade. With a remote
  grade, that changes the clip on every timeline sharing it. To try the plugin, switch
  the clip to a local version first.
- **To see the matte on the Color page:** set Output to Overlay or Matte.

## Slow-motion clips

The Fusion page works on source frames, not edit frames. A clip shot at 120 fps in a
24 fps timeline has about 5 source frames per edit frame: a 21-frame clip was 101 frames
to process. Check the frame count in Status, and use First Frame and Last Frame to try
a section first.

Mattes from an earlier First/Last Frame range stay in the cache, but two ranges
processed separately aren't blended where they meet. For a finished shot, process the
whole shot at once.

## Size and speed

Two limits decide what a Mac can process in one chunk:

- **Memory.** With the full 16-bit model, about 41 GB however small the clip, and
  about 44 GB for 33 HD frames, 51 GB for 73. Without the model's sound part about
  33 GB and 39 GB, and in 8-bit about 23 GB and 35 GB ([Weights](#the-controls)).
- **Chunk size.** macOS's GPU framework refuses the model's largest arrays above about
  170 megapixel-frames per chunk, however much memory is free. That makes 73 frames the
  practical maximum at HD.

So work in HD with a Resize, and use the matte as a mask on the full-resolution picture,
or use Guided ROI. UHD only fits for very short clips, about 17 frames.

| Mac | Job | Time |
|---|---|---|
| M5 Max, 128 GB | 30 HD frames in Resolve: 16-bit, 8-bit | about 4, 3.7 min |
| M5 Max, 128 GB | 176 HD frames in Resolve, 3 chunks | about 31 min |
| M5 Max, 128 GB | 176 frames with a 512×576 Guided ROI crop in Fusion Studio, 2 chunks | about 4.3 min |
| M5 Max, 128 GB | 63 HD frames with Frame Hold Off, 2x, 4x | about 9, 17, 35 min |
| M5 Max, 128 GB | 69 frames with a 512×608 crop in Fusion Studio, Crop Scale Off, 2x | about 1.2, 5.5 min |
| M1 Max, 64 GB | 32 HD frames (Auto: 16-bit without the sound part) | about 30 min |
| M1 Max, 64 GB | 81 HD frames in 73-frame chunks, the same | about 86 min per chunk |
| M1 Max, 64 GB | 32 HD frames in 8-bit | about 43 min |

On a 64 GB Mac, close other apps before Process. Auto plans on the Mac's size, less
about 8 GB for Resolve and macOS. If too much else is open, the memory check stops
and asks you to close apps, instead of letting the Mac swap to disk.

## Known limits

- **The matte follows the exact pixels.** The same frames give the same matte, but the
  model is very sensitive: a host can hand over slightly different pixels of the same
  clip (Resolve and Fusion Studio decode a .mov differently, and a decode can change
  between sessions), and the model then draws a slightly different set of strands. Both
  are valid, but for mattes that repeat exactly, feed the node an image sequence (EXR or
  DPX) rather than a movie file. The log shows a fingerprint of the frames each job
  received, so two renders can be compared.
- **Seams.** Long clips are made in chunks, and the model may pick a slightly different
  subject in each. Check the frames listed under Seams.
- **A tight guide next to a big object.** When the subject touches a large solid object
  the guide leaves out (a toy right beside a robot, for example), the model can invent
  shapes there for a few frames, such as a shifted copy of the subject. For subjects in
  a cluster like that, turn Guide With Alpha off and keep Multiply by Input Alpha on, or
  loosen the mask.
- **Matte haze shows in Pixel Spread.** Faint matte haze around the subject (a few
  percent) carries the subject's colour in Pixel Spread. Raise Alpha Black Clip a
  little, or guide the model with a mask.

## If something goes wrong

**Collect Logs** in Setup writes a support bundle (logs and system details) to your home
folder. The log itself is `~/.cache/ltx-alphagen/logs/ltx-alphagen.log`.

## Uninstall

Delete these. The last three hold the models, mattes and settings, and only exist once
you have used the plugin.

- `/Library/OFX/Plugins/LTXAlphaGen.ofx.bundle`
- `~/LTX-2.5` (or your Model Folder)
- `~/.cache/ltx-alphagen` (and your Cache Folder, if you changed it)
- `~/.config/ltx-alphagen`

## Windows (NVIDIA)

New in 1.50 and experimental. It is the same plugin, built for Windows 10 and 11 (64-bit) and
running the model with PyTorch on an NVIDIA card (CUDA). There is no installer yet: you
[build it from source](#build-from-source).

**What has been tested** (one PC: Windows 11, RTX 4090 24 GB, 94 GB of memory, Resolve Studio
21.2): Setup from start to finish (runtime, model download, prompt cache), and Process with the
real model on Resolve's Fusion page, in fp8 with the weights kept in system memory, which is
what Auto picks on a 24 GB card. On a synthetic clip whose true matte is known, the matte's mean
error was 0.0058 (the Mac measured 0.0054).

| Job (fp8, weights in system memory) | Time | Peak GPU memory |
|---|---|---|
| 120 HD frames in Resolve, 6 chunks of 33 | about 11.5 min (about 110 s a chunk) | 11.1 GB |
| 17 HD frames in Resolve, 1 chunk | 78 s | 7.6 GB |
| 17 frames at 768×512, outside Resolve | 50 s | 5.5 GB |

**What has not been tested yet:** bf16, the weights on the GPU (a card with more memory) or read
from disk, chunks longer than 33 frames, and real footage (the HD runs used a noise pattern, which
shows that it runs, not how good the matte is). Fusion Studio and the Edit and Color pages are
untested on Windows too. If something breaks, **Collect Logs** has what is needed to fix it.

**What you need**

- An NVIDIA card with a current driver. AMD and Intel cards can't run it.
- GPU memory: the model's weights are about 42 GB (bf16) or about half that (fp8), so they only
  fit the largest cards. On smaller cards the plugin keeps them in system memory and sends them
  to the GPU a layer at a time: that is how it was tested on a 24 GB card. It needs about
  26 GB of free system memory in fp8 or 48 GB in bf16; with less, they are read from disk, slowly.
- About 85 GB of disk space for the models and the runtime.
- Nothing else: no Python and no compiler. Setup brings its own Python.

**What is different from the Mac**

- **Install:** `.\install.ps1` copies the plugin to
  `C:\Program Files\Common Files\OFX\Plugins\LTXAlphaGen.ofx.bundle` (Windows asks for
  administrator rights).
- **Setup:** there is no Command Line Tools step. The Choose buttons open Explorer's folder picker.
  **Link Existing Models** needs Windows' Developer Mode for a real link; without it Setup makes
  a hard link when both folders are on one drive, and copies otherwise. There is no extra fp16
  copy of the model.
- **Performance** (a folded section, Windows only):

  | Control | What it does |
  |---|---|
  | Precision | **bf16**: reference quality. **fp8**: the weights take about half the memory, with a slightly different matte. New nodes start on fp8 when bf16 does not fit the GPU. Needs Process. |
  | Model Loading | Where the weights are kept. **Auto** (default): on the GPU when they fit its free memory, otherwise in system memory, otherwise read from disk. **On the GPU**, **In System Memory** and **From Disk** force one. The matte is the same. Needs Process. |

- **Chunk Size:** Auto can use up to 145 frames at HD when the GPU has the memory (the Mac's size
  limit does not apply).
- **Folders:** the same names, in your user folder: `%USERPROFILE%\LTX-2.5` (models),
  `%USERPROFILE%\.cache\ltx-alphagen` (mattes and the log, `logs\ltx-alphagen.log`),
  `%USERPROFILE%\.config\ltx-alphagen` (settings). Collect Logs writes a `.zip` to your user
  folder. To uninstall, delete those and the plugin folder above.

## Build from source

**Mac:** you need the Xcode Command Line Tools and cmake (`brew install cmake`). Quit Resolve first.

```bash
./build.sh --systest
```

```bash
./install.sh
```

**Windows:** you need Visual Studio 2022 or its Build Tools with "Desktop development with C++",
[CMake](https://cmake.org/download/), and Python 3.10 or later (or uv) on PATH. Quit Resolve first.
In PowerShell:

```powershell
.\build.ps1 -Systest
```

```powershell
.\install.ps1
```

If PowerShell says scripts are disabled, run them as
`powershell -ExecutionPolicy Bypass -File .\build.ps1 -Systest`.

The source is in `src/`, and every change from the Flame version is listed in
`src/CVT_PORT.md`.

## Licences and credits

- **The plugin** is open source under the Mozilla Public License 2.0 (`src/LICENSE`),
  the licence of the Flame plugin it is based on. See `src/NOTICE`.
- **The original plugin:** LTX Alpha Gen OFX for Flame, copyright 2026 Jan Klier, donated
  to and maintained by Logik.tv.
- **The models are not included.** Setup downloads them from Lightricks under the LTX-2.x
  Community License, which is free below USD 10M annual revenue. Above that, a paid
  Lightricks licence is required.
- **The port:** Chadwick, [Creative Video Tips](https://creativevideotips.com).

DaVinci Resolve and Fusion Studio are trademarks of Blackmagic Design.
