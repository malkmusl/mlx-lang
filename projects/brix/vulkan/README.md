# Our Vulkan driver for the RX 580: its shader code

brixOS is to get a complete Vulkan driver of its own for the AMD Radeon
RX 580 (Polaris 10, the card generation GFX8), written in Mlx, no Mesa
port. It follows a study of the driver written for this work (the
driver's stages, the card's command streams, the shaders, the kernel's
side, each held against Mesa's and Linux's sources; the study is not
kept in the repository, its section numbers are given here where it is
followed). This directory holds the first part, the one that needs no
card: the card's instruction set, a compiler from SPIR-V to it, and an
interpreter that runs the compiler's output on the host. Nothing here
touches the kernel or a card, and nothing is built into brixOS's image
yet: the files are a library the driver will use, built for the host by
the tests.

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

- (a) The driver is found and its device listed: `/devices/gpu0` and its
  record from the kernel, the loader's native path, instance and physical
  device queries. Nothing of this directory is needed yet.
- (b) Compute: std.gpu's kernels on the card, the desktop composing with
  it. From the kernel: memory, mapping, submit and fences on a compute
  ring. From the driver: devices, queues, memory, buffers, descriptor
  sets, compute pipelines (`shader.compile`), command buffers as streams
  with the preamble (its cache invalidation at every submit), dispatches.
  And the study's host card (its 8.1): a decoder of the command stream
  that keeps the registers the packets set and, at a dispatch, runs the
  shader in `wave.dispatch` with the entry state built from them. Then
  tests 254, 257, 263, 276 and the compositor's frame comparison run
  against our driver on the host. The self-tests at device creation (the
  doubler, the division) go to the card with it.
- (c) The first triangle into a buffer: vertex and fragment shaders
  (floats, vectors, exports, interpolation), a model of the fixed stages
  for the host card.
- (d) What std.gpu's clients use beyond compute: sharing, the display in
  place, timestamps, floats in compute, swapchains.
- (e) Vulkan 1.0 as required: textures, tiling, every mandatory format,
  the conformance suite; the compiler grown to SPIR-V 1.0's whole shader
  subset (spilling, calls, switch).
- (f) 1.1 to 1.3.
