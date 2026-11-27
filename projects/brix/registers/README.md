# The RX 580's registers, from the specifications

The Radeon RX 580 (Polaris 10: graphics GFX 8, memory controller GMC 8.1,
OSS 3.0, BIF 5.0, SMU 7.1.3, display DCE 11.2) as Mlx modules of ours,
made by `tools/gpu_registers_generate.py` from Mesa's register database
and Linux's register headers at fixed tags. No register's number, field
mask or enum value is typed by hand: code names it through its block's
module (`gmc_8_1.VM_L2_CNTL`), and the module is generated.

| Module | What it holds | From | A register is |
| --- | --- | --- | --- |
| `gfx8.mlx` | every register of Mesa's GFX8 database (1496) with every field (3443) | Mesa mesa-24.0.0 `src/amd/registers/gfx8.json` and the GFX8 entries of `registers-manually-defined.json` | its byte offset (the database's own form; Mesa and the Vulkan driver's packets count so) |
| `gfx8_enums.mlx` | the database's 96 enums, by its names | `gfx8.json` | (values) |
| `gfx_8_0.mlx` | the graphics block's registers SELECTION names (137), with every field (651); four enums | Linux v6.6 `asic_reg/gca/gfx_8_0_d.h`, `_sh_mask.h`, `_enum.h` | its dword index (Linux's mmNAME, the kernel's form) |
| `gmc_8_1.mlx` | the memory controller's (49, 246 fields) | `gmc/gmc_8_1_*` | dword index |
| `oss_3_0.mlx` | the interrupt ring, the host path and both copy engines (51, 301 fields; Linux has no sdma0/sdma1 headers for VI: these are theirs) | `oss/oss_3_0_*` | dword index |
| `bif_5_0.mlx` | the bus interface's (15, 46 fields) | `bif/bif_5_0_*` | dword index |
| `smu_7_1_3.mlx` | the SMC's, its index space's (`ixNAME`; 24, 91 fields) | `smu/smu_7_1_3_*` | dword index, or the index space's number |
| `dce_11_2.mlx` | the display's words the engines read and those the display's files name (118, 537 fields); MMIO_RUNS, the display's whole register space as runs | `dce/dce_11_2_*` | dword index |

A field is `NAME__FIELD` (its mask) and `NAME__FIELD__SHIFT` (its lowest
bit); an enum is a struct of its values (`gfx_8_0.MTYPE.MTYPE_UC`,
`gfx8_enums.ArrayMode.ARRAY_2D_TILED_THIN1`). The constants have no type
of their own, so a `u32` and a `usize` take them alike. Mesa's database is
generated whole; of Linux's headers (17 463 registers and 38 125 fields
in these six blocks) only what SELECTION names, each register with all
its fields.

SELECTION chooses every register the engines' files name and, ahead of
their move, those the display's files and the kernel's Vulkan calls name
with the header's number. The kernel's engines take them now:
`kernel/gpu_engines.mlx`, `gpu_smc_sdma.mlx`, `gpu_compute.mlx`,
`gpu_graphics.mlx`, `gpu_first_space.mlx` (each
`pub const NAME: usize = block.NAME`, the golden tables' registers by
Linux's names) and the tests' card `tests/support/engine_card.mlx` (the
display's runs). The kernel built from them is the kernel built before:
code, data, symbols and every line of the line table byte for byte; the
line table names the six modules more, and the ELF header's e_shoff
moves by as much (the boot image differs in those two bytes alone).

## How a new register is added

Never typed: generated.

1. Its name as Linux's header has it, without `mm` (a register of an index
   space with its `ix`), goes under its block in `SELECTION`; an enum of
   `gfx_8_0_enum.h` as `enum NAME`. A register of Mesa's database is in
   `gfx8.mlx` already.
2. `tools/gpu_registers_generate.py` (no arguments) writes the modules
   again; a name the header does not have is refused with its line.
3. The code names it through the block's module
   (`const gmc_8_1 = @import(".../registers/gmc_8_1.mlx")`).
4. `SELECTION` and the changed modules are committed together.

## How the generated files are checked

- `tools/gpu_registers_generate.py --check` makes every module again from
  the pinned inputs and compares it with the committed one; it ends with 1
  and names the first line that differs, an input that is not its pinned
  self (`upstream/SOURCES`), or a name of SELECTION no header has. Each
  module's head holds the sha256 of its inputs, so a changed input changes
  the module. `tests/520` runs it, and runs it on a changed input, a
  changed module, a wrong SELECTION and a missing one.
- `tests/519` holds the modules against the inputs read by a parser of its
  own (Mlx: the headers' `#define`s and enums, the database by std.json):
  every constant of every module, the selection's registers each with all
  its fields, the runs, and the cases where the database and Linux differ.
- `tools/check_gpu_registers.py HEADERS` stays the backstop for the
  kernel: that each constant of the engines' files names the generated one
  of its own name in the right block, that no register's number is typed
  there (neither as a constant nor where a register is read or written),
  that a golden table names Linux's register by name in Linux's order, and
  everything the generator cannot know: where the kernel uses a number
  (the values it writes, their order, Linux's numbers that are no
  register, no write to the display's registers).

## Where Mesa's database and Linux's headers differ

Every register, field and enum value of the database is held against
Linux's `gca/gfx_8_0` (and `oss/oss_3_0` for Mesa's own additions): 1464
registers at the same place, 3441 fields with the same bits, 622 enum
values the same. They differ, and both are kept apart (the database's in
`gfx8.mlx`, Linux's in `gfx_8_0.mlx`), for:

- the 32 SQ_WAVE_* registers (SQ_WAVE_MODE, _STATUS, _TRAPSTS, _HW_ID,
  _GPR_ALLOC, _LDS_ALLOC, _IB_STS, _PC_LO, _PC_HI, _INST_DW0, _INST_DW1,
  _IB_DBG0, _IB_DBG1, _TBA_LO/HI, _TMA_LO/HI, _TTMP0 to 11, _M0,
  _EXEC_LO/HI): the database puts each among the MMIO words at four times
  the number Linux gives it in the SQ's index space (`ixSQ_WAVE_MODE`
  0x11, reached through SQ_IND_INDEX and SQ_IND_DATA; the database's
  SQ_WAVE_MODE is 0x44). Their fields are the same in both.

What only one of them covers, with no number in question: two fields only
the database names (SQ_IMG_SAMP_WORD3.UPGRADED_DEPTH, bit 29;
SQ_THREAD_TRACE_MASK.RANDOM_SEED, bits 16 to 31); 754 registers the
database leaves without fields, to which Linux gives one field that is the
whole word; 11 enums only the database has, by names of its own
(CB_COLOR_DCC_CONTROL__MAX_UNCOMPRESSED_BLOCK_SIZE, ..._MIN_COMPRESSED_
BLOCK_SIZE, CLIP_RULE, EXCP_EN, FLOAT_MODE, PA_SU_SC_MODE_CNTL__POLYMODE_
FRONT_PTYPE, PA_SU_SC_MODE_CNTL__POLY_MODE, PA_SU_VTX_CNTL__ROUND_MODE,
QUANT_MODE, ROP3, VGT_HS_OFFCHIP_PARAM__OFFCHIP_GRANULARITY). The
generator prints all of it and writes it into `gfx8.mlx`'s head. Linux's
headers themselves name 27 masks of no register of theirs and give five
fields a mask without a shift or the other way round (the generator says
which; those fields are left out).

## The inputs

`upstream/` holds the inputs unchanged (6.5 MB): `linux-v6.6/asic_reg/`
the thirteen headers, `mesa-24.0.0/src/amd/registers/` the two JSON files;
`upstream/SOURCES` names each one's URL, tag and sha256, and the generator
refuses a file whose sha256 is not that. The Linux headers are AMD's,
under the permission notice at their heads; Mesa's database is MIT. A
newer tag is taken by putting the new files into `upstream/`, their
sha256 into SOURCES and the tag into the generator, and generating again:
the differences of the modules are then the review.
