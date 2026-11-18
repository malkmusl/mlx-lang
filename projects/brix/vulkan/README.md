# Our Vulkan driver for the RX 580: its shader code

brixOS is to get a complete Vulkan driver of its own for the AMD Radeon
RX 580 (Polaris 10, the card generation GFX8), written in Mlx, no Mesa
port. It follows a study of the driver written for this work (the
driver's stages, the card's command streams, the shaders, the kernel's
side, each held against Mesa's and Linux's sources; the study is not
kept in the repository, its section numbers are given here where it is
followed). This directory holds the first part, the one that needs no
card: the card's instruction set, a compiler from SPIR-V to it, and an
interpreter that runs the compiler's output on the host; and the driver's
stages (a), the device found and listed, and (b), compute (below), which
programs on brixOS link in.

## What stands

| File | Lines | What it is |
| --- | ---: | --- |
| `isa.mlx` | 1492 | The instructions GFX8 has, with their formats and numbers: 946, generated (below) |
| `encode.mlx` | 479 | An instruction as a record, and its words for the card; the operand numbers, the inline constants, the wait word |
| `decode.mlx` | 699 | The words back into the record, and the record as text in LLVM's syntax (the disassembler) |
| `wave.mlx` | 1669 | The interpreter: a wave of 64 lanes, a compute dispatch started as the card starts it |
| `shader_ir.mlx` | 1155 | The compiler's front: SPIR-V read into SSA values with block parameters, division lowered, uniform and varying values marked |
| `shader_code.mlx` | 1435 | The compiler's back: selection, control flow on the exec mask, registers, waits, wait states, assembly, the shader's facts |
| `shader.mlx` | 72 | The compiler as the driver calls it: `shader.compile(spirv, allocator, words, &facts, &failure)` |

What the compiler takes at this stage: a GLCompute shader with one
function, 32-bit integers and booleans, function variables, push constant
words, storage buffers of words (set 0, any binding), the built-ins
GlobalInvocationId, LocalInvocationId and WorkgroupId, SPIR-V's integer
arithmetic, comparisons and logic, selections, and loops whose condition
is at their top. That is what std.gpu's nine kernels use. Everything else
is refused with the opcode and the word where it stands (no floats in
SPIR-V yet, no switch, no break, no function calls, no spilling, no
boolean of a loop's header read after the loop).
Division and remainder by a variable are Mesa's reciprocal sequence
(nir_lower_idiv.c's emit_udiv and emit_idiv), by a constant power of two a
shift or a mask.

The shader's entry state is the study's own choice for pointers (its
5.2, the second way out), so the driver must start a dispatch this way:

    s0, s1      the scratch descriptor (never read: nothing spills)
    s2, s3      descriptor set 0's 64-bit address; binding b's buffer
                descriptor (four words) at byte 16 * b
    s4, s5      the push constant block's 64-bit address
                (USER_SGPR 6)
    s6, s7, s8  the work group's id x, y, z (TGID_X_EN .. TGID_Z_EN all on)
    v0, v1, v2  the invocation's id in its group (TIDIG_COMP_CNT 2)

`shader.Facts` says the rest the driver writes into the dispatch's
registers: the number of words, the vector and scalar registers used, the
group size, the push words and bindings read.

## The driver, stage (a): the device is found and listed

| File | What it is |
| --- | --- |
| `card.mlx` | The driver's one interface to the kernel (the study's 8.1): the calls of brixOS's native interface for a card, as a record of functions |
| `card_brix.mlx` | Its brixOS implementation: the native calls through `std.gpucard` |
| `card_host.mlx` | A card on the host, its second implementation: the same statuses, handles and record (the kernel's own encoder, `projects/brix/kernel/gpu_record.mlx`), answered in the process; later stages run its streams and shaders here |
| `driver.mlx` | The Vulkan driver: the loader/driver interface, the instance, the physical device's queries, the device and its queue |

The kernel's side (spec/04-stdlib/os/brix.xml, `<GpuDevices>`, calls 80
to 85; `std.gpucard`): the card is `/devices/gpu0`, whose read gives its
record (what it is, where it sits on PCI, its memory, the ROM's counts of
its graphics block, the rings that take commands and why there are none);
opening it to write gives a handle of the kind gpu, which is the
process's context on the card. Memory for the card, its addresses,
submits and fences are specified and refused for now (unsupported): the
kernel starts none of the card's engines yet.

How a program reaches the driver: x86_64 writes no shared objects, so the
driver is a module of the program (the study's decision 3). The program
offers it (`driver.offer(card_brix.interface())`), and
`std.vulkan.loader`'s native path lists it first, as `builtin:brix-radeon`,
while it has a card, and opens it by its entry points with the interface
negotiated as for any driver. `vkinfo` (projects/brix/tools/vkinfo, in the
image) and the compositor (its `card.brixos.mlx`) do so.

What it answers: the instance is Vulkan 1.1 and answers every
instance-level command of 1.0 and 1.1 (all of them queries); at stage
(a) the device had `vkDestroyDevice` and `vkGetDeviceQueue` alone (stage
(b), below, adds its commands); every other command is not exported
(the loader gets 0, never a wrong function).
What the device says is only what works: every feature false (also
robustBufferAccess, 1.0's one required feature, until the card shows it
checks a buffer's range: the study's 11.5), no format feature and no
image format until stage (c), one queue family (compute and transfer, one
queue, no timestamps), the memory from the card's record in the
reference's types on up to three heaps (the system's memory only while
the card's GART is on, no heap without bytes), the reference's limits for this
generation, and the stage in the driver's information string.
`vkCreateDevice` is refused (VK_ERROR_INITIALIZATION_FAILED) while the
card's record lists no compute ring; on the host card that lists one it
opens a context on the card, which the device holds.

On the user's machine `vkinfo` lists the RX 580 (its name, ids, memory,
compute units) and says that no device can be made yet; the compositor
says in its log that it found the device and composes on the processor,
as before. Without the card (QEMU) both say there is no Vulkan device.

The proof: `tests/502_gpu_card_interface_runtime.mlx` (the interface's
calls and statuses, and the record against the spec, on the host card),
`tests/503_vulkan_driver_entry_runtime.mlx` (every entry point against
what the registry, std/registry/vulkan/vk.xml, requires of it), and
`tools/check_brix_vulkan.sh` (brixOS in QEMU, without the card). The
driver's numbers (the limits, the Polaris 10 device numbers) are held
against Mesa's radv_physical_device.c and radv_constants.h and Linux's
amdgpu_drv.c by `tools/check_vulkan_driver.py MESA LINUX`, a development
check that needs those files fetched.

## The driver, stage (b): compute

std.gpu's commands (the study's 8.2 (b)): its kernels run through our
driver, proven on the host card, whose submits are executed.

| File | What it is |
| --- | --- |
| `device.mlx` | What the parts share: the card interface, the device and queue objects, host memory, the card's memory the driver makes for itself and the card addresses it chooses (code below 4 GiB, everything else above) |
| `memory.mlx` | vkAllocateMemory (a memory handle of the card, mapped at the device's address, zeroed), vkMapMemory (the processor's mapping, kept), vkFreeMemory; buffers and their binding |
| `pipeline.mlx` | Shader modules, descriptor set layouts, pools and sets (16-byte buffer descriptors in the card's memory, binding b at 16 * b), pipeline layouts, compute pipelines (our compiler; the code below 4 GiB at 256 bytes; the 15 register words of the study's 4.3 A) |
| `commands.mlx` | Command pools and buffers as growing streams of packets; bind, push, dispatch, barrier, copy and fill as RADV writes them on a compute ring of GFX8 |
| `queue.mlx` | The preamble (the study's 4.2, its cache invalidation at every submit), submits, fences, the queue and device idle |
| `selftest.mlx`, `doubler.mlx`, `divider.mlx` | The doubler and then the division by a variable (64 quotients and remainders) at every vkCreateDevice, held against the processor's; a mismatch refuses the device and says the first wrong word |
| `packets.mlx` | The packets and registers of a compute stream |
| `card_run.mlx`, `card_cache.mlx` | The host card's engine: a submit's streams executed and their shaders run in `wave.mlx`; the card's caches modelled |

The device is Vulkan 1.1 (as the study has it from stage (b)) and exports
the stage's 46 device commands; every other command answers 0. Its own
1.1 commands (vkGetDeviceQueue2 and the others) are not among them yet.

WHAT A DISPATCH WRITES (the study's 4.3): the pipeline's 15 words when it
changed, the flush a barrier left owing, set 0's address into user data 2
and 3 and the push constants' block into 4 and 5 (64-bit pointers in even
pairs: 5.2's second way out, which the compiler expects), and
DISPATCH_DIRECT with the initiator 0x45. COMPUTE_RESOURCE_LIMITS is the
card's record's: the user's card has 36 compute units in 4 engines, 9 an
engine, so FORCE_SIMD_DIST for a group of one wave (0x00800000); a record
that does not count them refuses the device. A barrier writes nothing
where it stands but the copy engine's wait; its flush (RADV's stage and
access rules, the graphics ones dropped on a compute queue) is owed to the
next dispatch, copy or the end: std.gpu's barrier becomes CS_PARTIAL_FLUSH
and ACQUIRE_MEM 0x08C40000. Copies and fills are the processor's copy
engine of the compute queue (DMA_DATA, 7.6) in packets of at most
2 097 120 bytes, RAW_WAIT on the first after a flush, CP_SYNC on a fill's
last packet and at the end of a command buffer that copied.

THE HOST CARD (card_host.mlx) answers calls 80 to 85 when its record lists
a compute ring, and executes a submit: card_run.mlx walks the packets,
keeps the registers (for one submit: the preamble sets them every time),
does the preamble's and the barriers' cache actions, runs each dispatch's
shader in the interpreter wave by wave with the entry state of 4.3 and
5.2, and the copy packets; anything it does not know (a packet, a register
outside the four ranges, a register the dispatch reads that no stream set,
a value it does not model, code not mapped executable, a stream not padded
to 8 words) loses the context with the stream and word where it stands.
card_cache.mlx models the instruction, scalar and vector caches and L2: a
read whose cached word is older than memory's is refused as stale, a word
a dispatch wrote is in flight until a CS_PARTIAL_FLUSH, a word the copy
engine wrote until a CP_SYNC or RAW_WAIT, and the kernel's fence writes L2
back. So a missing invalidation or wait shows on the host. V stands for
every compute unit's L1 at once: a word one dispatch stored and a later
one reads before TCL1 lets the L1s go is stale (its waves may run on
another unit); a word a dispatch may still be writing stays in flight
when L2 is let go (ACQUIRE_MEM without CS_PARTIAL_FLUSH before it is a
race); and of the scalar registers RSRC1 gives, the last two are VCC's
(ACO's get_addr_sgpr_from_waves).

The self-test divides too (divider.mlx): the desktop's kernels divide by
variables on their first frame, our compiler does it with Mesa's
reciprocal sequence, and the card's v_rcp_f32 is not the host's (the
study's 11.14): a reciprocal one unit high gives wrong quotients, and the
device is refused with the first one.

What the tests found against the study: std.gpu's own frames do not need
the preamble's scalar cache invalidation, because each of its dispatches
is followed by a barrier with a shader read, whose flush (RADV's
radv_dst_access_flush: ACO loads storage buffers by SMEM) invalidates the
scalar cache at the frame's end, before the processor writes the next
frame's sets; a frame without such a barrier at its end does need it
(tests/509). The preamble keeps it at every submit, as RADV does.

On brixOS the kernel's record lists no compute ring until its calls 80 to
85 are served, so vkCreateDevice is refused there as before: vkinfo says
"not yet: no engine of the card takes commands" and the compositor
composes on the processor. Where a device opens (its self-test passed),
the compositor says so and still composes on the processor: the brixOS
backend offers the Vulkan renderer no output yet.

The proof: `tests/508_vulkan_host_card_runtime.mlx` (the host card: the
calls, streams built by hand word for word as the study's 4.2 and 4.3
give them, every refusal with its stream and word, the caches, the copy
packets) and `tests/509_vulkan_compute_driver_runtime.mlx` (the driver
through the loader: each command against the registry's codes, the words
a dispatch and a barrier write, the six silent needs of the second
reading, std.gpu's nine kernels through std.gpu word for word against the
processor with tests/497's cases, tests 253, 254 and 263 over our driver,
the self-test passing and failing on a wrong compiler output and on a
reciprocal a unit high). The
numbers of `packets.mlx` and the address spaces are held against Mesa's
and Linux's files by `tools/check_vulkan_driver.py MESA LINUX` (its
head names the files).

## Where the numbers come from

Every number of the card in these files is from Mesa at the tag
mesa-24.0.0 (src/amd/compiler's aco_opcodes.py, aco_assembler.cpp,
aco_ir.h, aco_ir.cpp, aco_insert_NOPs.cpp, aco_insert_waitcnt.cpp,
aco_validate.cpp; src/compiler/nir's nir_lower_idiv.c and nir_opcodes.py;
src/amd/registers/gfx8.json), none from memory. Mesa's files are not kept
in the repository; they are fetched into a directory of one's own from
`https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-24.0.0/PATH`.

`isa.mlx` is made by `tools/gcn_isa_generate.py` from aco_opcodes.py
(parsed, never run): the GFX8 column only, never the GFX9 one or ACO's
merged one. 946 instructions: the study's corrected 762 rows of the table
(832 with a GFX8 number, less GLOBAL's 48 and SCRATCH's 22, which ACO
emits only from GFX9 on) less FLAT's 8 d16 forms (FLAT has only the
file's shared "GFX89" column; MUBUF's own GFX8 column has no d16 form,
and llvm-mc for this generation refuses them), and the 192 comparisons
the file makes in loops.
The file's header carries the tag and the sha256 of the aco_opcodes.py it
was made from.

    python3 -I tools/gcn_isa_generate.py MESA/src/amd/compiler/aco_opcodes.py

Two development checks hold the files against their sources; neither is
needed to build:

- `tools/check_gcn_isa.py MESA` reads Mesa's files by its own means and
  holds `isa.mlx` (every instruction, its format and number, the count,
  the hash), `encode.mlx`'s constants (each format's fixed bits, the
  special registers, the inline constants, the VOP3 offsets, the wait
  word) and `wave.mlx`'s and `shader_ir.mlx`'s numbers (the wait-state
  counts, the division's scale) against them.
- `tools/check_gcn_encoding.sh [compiler]` gives LLVM's assembler for the
  card (`llvm-mc -triple=amdgcn -mcpu=tonga`, the same generation as
  Polaris) every instruction the compiler emits for the nine kernels and
  every one its selection can emit in a few operand patterns
  (`tools/gcn-encoding/main.mlx` lists them): LLVM must make our words of
  our text, and our text of our words. Where llvm-mc is not installed it
  says so and passes.

## How the proof works

Three tests, each built for the host by `tests/run_std_modules.sh`:

- `tests/495_gcn_encoding_runtime.mlx`: an instruction of every format
  the card has (SOP2 to EXP, VOP3 with its modifiers and the carry forms,
  DPP and SDWA) encoded and held against the words llvm-mc gave for it,
  written into the test as numbers; decoded and named back; every one of
  the 946 encoded and decoded again; the inline constants' edges; what may
  not be written (a number GFX8 lacks, a literal in VOP3, a scalar
  register as a VOP2 instruction's second source, a scalar load's base at
  an odd register, a buffer descriptor not at a multiple of four, offsets
  too large) and words that are no instruction.
- `tests/496_gcn_interpreter_runtime.mlx`: each instruction's effect on a
  wave, with the carries and flags at their edges, exec and the saveexec
  forms, a comparison's lanes that are off, the loads and stores, a work
  group's start; and what the interpreter must refuse: each of the ten
  wait-state rules of GFX8 one state short (and passed with its s_nop), a
  loaded register read before its wait, two scalar values on one vector
  instruction, a buffer access past its range or outside memory, a
  register nothing wrote, a value the sources do not give, an instruction
  not modelled.
- `tests/497_gcn_compiler_runtime.mlx`: std.gpu's nine kernels built as
  SPIR-V by std.spirv.builder, compiled here, and run in the interpreter on
  real inputs, every work group and lane, held pixel for pixel against the
  processor: the text against std.truetype.drawRun, nv12 against
  std.gpu.video.convertNv12, paint against std.ui.canvas, blit against the
  compositor's CPU renderer (scene.mlx); pattern, both blurs, warp and
  shrink, which the processor renderer draws another way, against the
  kernels' arithmetic written out plainly. Then divisions at the edges and
  at chance, signed and unsigned, by variables, by powers of two and of two
  uniform values, against the processor's; what a loop's header makes,
  read after the loop (a word: each lane keeps its own; a boolean:
  refused, its lane mask is made whole in every round); each of the ten
  comparisons on a lane's values and on the wave's, and an if on a lane's
  value whose two sides give the wave's values; and the wait states the
  compiler's pass inserts.

The interpreter runs the words, not the compiler's records, and refuses
rather than guesses: what the sources do not say of the card stops a run
and names the place (wave.mlx's head lists each case). It also keeps the
rules the card does not check: the waits for loads, the ten wait-state
rules, the constant bus. So a test that passes says the words obey them.

The tests were checked for teeth: 62 deliberate mistakes were put into
copies of these files (wrong bits in each format, wrong flags and edges in
the interpreter, each wait-state rule one short, the division's signs and
refinements, the compiler's selection, control flow, registers and
branch distances, and the two bugs the tests found while this was
written). 58 are found. The four that are not change nothing the tests
can reach: a conversion bound that rounds to the same float; the
division's first refinement on `>` instead of `>=` (the second catches
it); the division's scale 2^32 - 256 instead of 2^32 - 512 (it differs
only for a reciprocal more than half and less than one unit off, which
the interpreter's whole units cannot show); and the constant-bus guard of
the selection, which no value of this stage reaches (only a multiply-high
of two uniform values would, and the reader never makes one).

## What the card must answer

The sources leave these open; each is a question of the study's section
11, to be asked of the card with a log line before its kernels are
trusted:

- The division (11.14): the reciprocal sequence holds with the host's
  v_rcp_f32 and with one or two units in the last place below it, but one
  unit above breaks it (2 / 3 gives 3, and 1 / 1 reaches 2^32, which
  v_cvt_u32_f32 must then convert: what the card gives for that is not
  in the sources). The card's v_rcp_f32 must not round up; the self-test
  must include 2 / 3 and divisors of 1.
- The entry state (11.2): the probe must be written for the layout above
  (the group ids from s6, not s4).
- Buffer ranges (11.5): the interpreter stops at an access past the
  descriptor's range; that the card gives 0 and drops the store is the
  probe's to show.
- What a comparison writes in the lanes that are off: not said; the
  interpreter writes ones there, so that code counting on zeros fails
  here first.
- Whether s_endpgm waits for outstanding stores by itself: not said; the
  compiler always waits explicitly.

## Next

The study's section 8 has the stages; this directory is the compiler and
the interpreter of stage (b), built ahead of it.

- (a) The driver is found and its device listed: built (above).
- (b) Compute: the driver's side built (above), proven on the host card.
  What remains: the kernel's calls 80 to 85 on a compute ring (after its
  stages 5 to 7), then the card's own answers to the study's section 11
  (the entry state, the division's reciprocal, the buffer range check)
  with a log line each, and the brixOS backend offering the Vulkan
  renderer its output. Test 257 needs memory shared as dma-bufs and host
  memory imported, stage (d)'s; 256 is the manifest's path, which a
  driver linked in does not take; 276 builds a module and runs no
  driver.
- (c) The first triangle into a buffer: vertex and fragment shaders
  (floats, vectors, exports, interpolation), a model of the fixed stages
  for the host card.
- (d) What std.gpu's clients use beyond compute: sharing, the display in
  place, timestamps, floats in compute, swapchains.
- (e) Vulkan 1.0 as required: textures, tiling, every mandatory format,
  the conformance suite; the compiler grown to SPIR-V 1.0's whole shader
  subset (spilling, calls, switch).
- (f) 1.1 to 1.3.
