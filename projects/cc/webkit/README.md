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
