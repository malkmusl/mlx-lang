# Status notes (2026-11-02)

Where the work stands, what was not tested on real hardware, and what
comes next. Kept short; the details are in the commits, PORT.md and
ROADMAP.md.

## Video and media (Voyager)

Done, tested here (4-core cloud container, lavapipe: no video GPU):

- H.264 on the CPU (std.h264.decode): CAVLC/CABAC, I/P/B, High,
  conformance-verified, frame-threaded (`Pool`); the player's fallback
  when no device decodes video (`MLX_VIDEO_DECODER=cpu|gpu`).
- H.265 parser (std.h265) and Vulkan H.265 decode (std.gpu.video,
  `decodeH265`), MP4 `hvc1`/`hev1`.
- H.265 on the CPU (std.h265.decode, commit 6aef29a): Main and Main 10,
  all 143 Main/Main 10 streams of the ITU HEVC_v1 suite bit-exact,
  frame-threaded; 1080p about 7 pictures a second on one thread, 12 on
  three. The player uses it whenever the GPU's decoder does not take a
  stream. tests/424, check_voyager scenario "hevc".
- Voyager's Backspace crash on YouTube's search box (commit d76b100):
  the page settings' generic font families were empty, so
  `-webkit-body` was a null family. Scenario 27 covers it.

Not tested on real hardware (to do at home, RX 580 with RADV's UVD):

- Vulkan H.264 and H.265 decode after the refactor that added H.265
  (`examples/vulkan-decode`, see PORT.md "On a machine with a video
  decoder").
- Main 10 on the GPU (10-bit session, G10X6_B10X6R10X6_2PLANE_420, rounded
  to 8 bits on read-back; UVD6 may refuse it, the player then falls back
  to the CPU).
- YouTube in Voyager with the fixes (search box, Backspace, playback).

Commands:

    RADV_EXPERIMENTAL=video_decode mlx-out/bin/vulkan-decode --trace test.mp4 out.png 50
    ffmpeg -i some.mp4 -c:v libx265 -pix_fmt yuv420p10le -tag:v hvc1 hevc10.mp4
    RADV_EXPERIMENTAL=video_decode mlx-out/bin/vulkan-decode --trace hevc10.mp4 hevc10.png 50
    ./mlx build webkit-mlx-voyager && tools/check_voyager.sh

## Video: what comes next

1. VP9 (task 91): std.vp9 reads the uncompressed header, superframes and
   reference slots (commit 39131eb, tests/425). Next is std.vp9.decode:
   bool decoder, compressed header (probability updates), partitions and
   modes, motion vectors, the 8-tap filters with scaling, DCT/ADST to
   32x32, loop filter, backward probability adaptation; then a Pool like
   H.265's. Verify against libvpx's test vectors (per-frame MD5s,
   https://storage.googleapis.com/downloads.webmproject.org/test_data/libvpx/).
   The tables can be generated from the standard's data (default
   probabilities, coefficient probabilities, scans, quantizers).
2. WebM/Matroska: an EBML reader feeding std.mp4's Movie/Track/Sample
   shapes (std.mp4.parse could dispatch on the EBML magic), so that the
   player and MSE take WebM unchanged; `video/webm` with `vp09`.
3. Vulkan VP9 decode: std/registry/vulkan/video.xml has the VP9 std
   headers, but they are not materialized into std.vulkan.video yet.
4. AV1 (task 92): CPU decoder, Vulkan AV1 decode, `av01` in MP4/WebM.
5. Opus (YouTube's WebM audio): std.mp4 knows the codec, no decoder yet.

## Other open tasks (from the task list)

- ELF writer: versioned dynamic symbol references (verneed).
- Itanium ABI: reuse tail padding of non-POD base classes.
- mlx4 gaps: i128 if-expression upper half and literal sign; narrow
  integer shifts and `~`; `f(x.?)` as a call argument; u128 constants
  above 2^64; `extern("c") var`.
- mlx-cc: `std::vector<std::tuple<long, Str>>::push_back` corruption.
- Running jsc on scripts, WebKit's build steps and code generators in Mlx,
  the WPE configuration, libpas back as WebKit's allocator.
- Convert the repository to std's containers, std.heap and `@noncopy`.

## brixOS kernel

On the branch `brix-kernel` (projects/brix/README.md there): mlx4 has an
`x86_64-freestanding` target (`--target=x86_64-freestanding`), brix-image
makes a Multiboot image (boot blob, kernel, file archive with mlx-sh and
the coreutils), and the kernel boots in QEMU to the serial console and
runs mlx-sh in user mode with Linux's system call numbers; programs run
as processes (fork, execve, wait4, pipes, Ctrl-C), scheduled when they
wait (no timer yet), on a VFS with a tmpfs (/bin is the archive, /tmp,
/root, /dev); copy on write is the next step (`./mlx build brix`, `tools/check_brix.sh`). Note:
ROADMAP.md is written for a Linux kernel ("replacing it is not the
goal"); brixOS is the Stage 4 target of AGENT_IMPLEMENTATION.md and
spec/04-stdlib/os/brix.xml.

To try at home:

    ./mlx build brix
    qemu-system-x86_64 -m 512M -cpu max -display none -serial stdio \
        -kernel mlx-out/bin/brix/brix.img
