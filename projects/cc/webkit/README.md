# WebKit patches

Changes to the WPEWebKit sources mlx-cc needs, applied to a WPEWebKit
2.54.1 tree with `patch -p1 < FILE` from its top directory.

- `0001-simdhelpers-portable-without-sse2.patch`: `wtf/SIMDHelpers.h`
  takes its x86-64 paths, which call SIMDe's conversions to native
  `__m128i`, only when SIMDe uses the SSE2 instructions themselves
  (`SIMDE_X86_SSE2_NATIVE`); otherwise the portable NEON-emulation
  paths it has for other CPUs. mlx-cc has no vector registers, so SIMDe
  is built with `-DSIMDE_NO_VECTOR` and emulates the vectors with
  arrays.
- `0002-libpas-portable-deallocation-check.patch`: libpas's
  double-free check on x86-64 (`pas_segregated_page_inlines.h`) is a
  hand-written `btrl`/`jc`/`call` asm block; under mlx-cc (`__mlxcc__`)
  it takes the portable C path the ARM and RISC-V builds use. mlx-cc's
  inline asm knows single instructions (fences, `cpuid`, `rdtsc`,
  `xgetbv`, `pause`), not assembly programs.
