# The Vulkan conformance suite on our toolchain

Khronos' Vulkan conformance suite (VK-GL-CTS, `deqp-vk`) at
vulkan-cts-1.3.10.0, built by this repository's own toolchain alone and
run against our Vulkan driver (projects/brix/vulkan) over the host card
(card_host.mlx). Like projects/webkit, the port is our files around
sources we do not edit: the sources are not in the repository at all
(UPSTREAM.md: which tag and why, the commits and trees, what is not
fetched), only what the port adds.

It is a measuring tool of the development host and nothing more. The
suite is Khronos' C++ and runs as a program of the host (Linux) against
the driver over the host card; nothing of it goes into the brixOS image,
and nothing of the driver depends on it. The driver itself is Mlx, and
on brixOS it reaches the card through brixOS's native calls alone (80 to
85: std.gpucard over std.os.brix), with no C, no C++ and no Linux call
in its way. What mlx-cc and mlxlibcpp grew for the suite (C++
exceptions, the stream and container headers) is there so that the
suite is built by our toolchain rather than a foreign one.

    tools/fetch_cts.sh                 the sources, at UPSTREAM.md's commits,
                                       into $CTS_SOURCES (mlx-out/cts/sources)
    ./mlx build cts                    mlx-out/cts/bin/deqp-vk and
                                       mlx-out/lib/libmlxcts-driver.so
    tools/run_cts.sh 'dEQP-VK.info.*'  a group run; its result file and summary
    tools/check_cts.sh --framework     the framework's self-tests (tests/521)
    tools/check_cts.sh                 build, then check-cases.txt (tests/522)

## What is built, and by what

Everything that is built is compiled by mlx-cc and linked by mlx-ld, on
mlxlibc (its loader and C library) with mlxlibcpp; no compiler, linker or
C library of the host's is in what is built. `build.mlx` describes it in
std.build (`./mlx build cts`, 257 steps, about 30 s on four cores with the
toolchain at hand):

- the framework: delibs (debase, depool, dethread, deutil, decpp,
  destream), qphelper, common (tcu), xexml, the reference renderer, the
  random shader generator; the GL utilities only as far as the Vulkan
  ones need them (port/gluWithoutGL.cpp in place of gluShaderProgram.cpp,
  which pulls in the GL wrapper);
- the Vulkan utilities (external/vulkancts/framework/vulkan) but
  vkRayTracingUtil.cpp (below);
- the test module's parts for the groups below, and vktTestPackage.cpp
  as it is: port/vktMlxGroups.cpp puts the groups together and names
  every other group with no case in it;
- glslang (48 files, with HLSL, its Unix OS layer; its C interface is
  left out), the suite's GLSL to SPIR-V: it is plain C++, and the
  programs of the groups are GLSL.

The groups, in the order they are brought up:

    dEQP-VK.info.*                         dEQP-VK.api.smoke.*
    dEQP-VK.api.info.*                     dEQP-VK.api.device_init.*
    dEQP-VK.api.object_management.single.* dEQP-VK.memory.allocation.basic.*
    dEQP-VK.compute.pipeline.basic.*       dEQP-VK.api.buffer.*
    dEQP-VK.synchronization.basic.*

(`compute.basic` of older tags is `compute.pipeline.basic` in 1.3.10.)

## What is not built, and why

- **SPIRV-Tools.** Its build makes its grammar tables with Python
  scripts from SPIRV-Headers' grammars; this repository has no generator
  of its own for them yet. port/spirv_tools.cpp stands in for its API:
  assembly and disassembly throw tcu::NotSupportedError (a case with
  SPIR-V assembly cannot run: its program is "not found"; the shader
  log has no disassembly), validation answers SPV_UNSUPPORTED, the
  optimizer does nothing. tools/run_cts.sh runs the suite with
  `--deqp-spirv-validation=disable --deqp-log-decompiled-spirv=disable`.
  Next: an Mlx generator of the tables (SPIRV-Headers' JSON grammars)
  and SPIRV-Tools' sources built like glslang's.
- **Amber.** port/vktAmberAbsent.cpp: every Amber case is not supported.
- **GL, EGL, window systems.** The platform (port/tcuMlxPlatform.cpp)
  has no window system; `--deqp-compute-only=enable`.
- **tcuImageIO.cpp**: it reads PNG files through libpng's sequential
  reader (png_set_read_fn), which mlx-png has not; nothing of these
  groups reads an image file.
- **vkRayTracingUtil.cpp**: mlx-cc does not find a function template
  whose trailing return type names a member of a dependent class
  (`auto f(Y) -> typename X<Y>::type`) at its call.
- **The prebuilt binaries** (`vulkan/prebuilt`): the programs are built
  when a case runs.
- **deTimer.c** (and so the framework's dE-IT.deutil.timer self-test,
  the one of modules/internal's delibs self-tests that tests/521 does not
  run): it uses POSIX timers (`timer_t`, `struct sigevent` with
  SIGEV_THREAD, timer_create), which mlx-cc's headers and mlxlibc have
  not; nothing the port builds calls it.

## How the suite reaches the driver

Linked in, not as an ICD: the suite is the loader here. Our driver is a
library of Mlx (port/cts_driver.mlx, port/driver.mlx: the user's RX 580
as the host card serves it with one compute ring, negotiated with as
std.vulkan.icd's loader would), deqp-vk is linked against it, and the
platform takes vkGetInstanceProcAddr from it (its header, MlxCtsDriver.h,
is made by mlx-cheaders from port/api/MlxCtsDriver.mlx). An ICD would
need a Vulkan loader under the suite; ours (std.vulkan.loader) is Mlx
and speaks to drivers natively, so going through it adds nothing the
suite checks. `--deqp-vk-library-path` still loads another Vulkan
library, to hold the suite itself against another driver.

Every command reaches the driver through an export of the library
(driver.mlx says why: an Mlx function finds its arena in registers C++
does not keep). The exports pass twelve words: a command with a float
by value (vkCmdSetLineWidth, vkCmdSetDepthBias, vkCmdSetDepthBounds and
two of extensions) cannot go through them, and there are 128 of them
(the driver's stage c has 118 commands; Vulkan 1.0 and 1.1 have 165).
Such a command is given to the suite as absent and the error output
says so (`mlxcts: the driver's NAME is not given to the suite: ...`);
tools/run_cts.sh repeats it in the summary and counts the run as
failed. When vkCreateComputePipelines answers VK_ERROR_UNKNOWN
because the driver's compiler refused a shader, the suite's error output
says why (`mlxcts: the driver's compiler refused the shader: ...`, with
the SPIR-V opcode and word).

## Results

tools/run_cts.sh writes each group's result file (the suite's .qpa) and a
summary: passed, failed with the first line of each message (a case that
ends the suite is told as a crash, and the suite runs again on the
cases after it; a case of the group or list the suite never began is
failed as not run), not supported with each reason's count, and how
many of those are the port's (Amber, SPIRV-Tools: "in this port"), not
the driver's. The suite runs with its watchdog, so a case that hangs
ends as a timeout. Every result file names the suite's release
(`#sessionInfo releaseName vulkan-cts-1.3.10.0`, made by build.mlx
from UPSTREAM.md). The cases of check-cases.txt pass and must keep
passing (tests/522, which also holds the reading of a result file and
tools/run_cts.sh against made-up results and a stand-in suite). What
fails and why is the driver's next work; the summary of a run is the
list.
