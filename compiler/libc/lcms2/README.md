# mlx-lcms2

Little CMS 2 for the C and C++ programs built here, in Mlx, with the API
of Little CMS 2.14 (`<lcms2.h>`, LCMS_VERSION 2140). WebKit's image
decoders (WebCore's PNGImageDecoder and JPEGImageDecoder, `USE_LCMS`)
include it as they would Little CMS's header and link `libmlxlcms2.so`
in place of `liblcms2.so.2`: an image's embedded ICC profile is opened
from memory, linked to the built-in sRGB profile, and each decoded row
transformed in place (BGRA, relative colorimetric).

The engine is the standard library's `std.colorprofile`
(std/src/colorprofile.mlx and std/src/colorprofile/), which Mlx programs
use directly, Little CMS step for step, so that its pixels are Little
CMS's to the bit:

- `curve.mlx`: tone curves (cmsgamma.c): segments of the parametric types
  (the ICC's five, Little CMS's 6, 7, 8, 108 and 109, their inverses) or
  sampled points, the 16-bit table of 4096 entries made of them, the
  fixed-point interpolation, reversal, linearity and monotony, the gamma
  estimate.
- `interp.mlx`: grid interpolation (cmsintrp.c): one input, and three
  inputs tetrahedral or trilinear in 16-bit fixed point, C's 32-bit
  overflow included.
- `pipeline.mlx`: stages (curves, matrices, 16-bit grids, Lab <-> XYZ, the
  Lab V2/V4 matrices) and pipelines evaluated in floats (cmslut.c).
- `matrix.mlx`: 3 x 3 matrices, the Bradford adaptation, primaries to
  XYZ (cmsmtrx.c, cmswtpnt.c).
- `profile.mlx`: the header and tag directory, `XYZ `, `curv`, `para`,
  `sf32`, `mft1`, `mft2`, `mAB `, `mBA ` (cmsio0.c, cmstypes.c), and the
  profiles made in memory (sRGB, RGB, grey, XYZ, Lab; cmsvirt.c).
- `link.mlx`: the intents (cmsio1.c, cmscnvrt.c): matrix-shaper and LUT
  pipelines in and out of the PCS, the Lab V2/V4 encodings, absolute
  colorimetry, black point compensation with Little CMS's black point
  detection (cmssamp.c).
- `optimize.mlx`: cmsopt.c's optimizations, all of them, since each one
  changes the numbers: the matrix-shaper in 1.14 fixed point, joined
  curves, the prelinearized 8-bit grid, the resampled grid and its white
  fixed.
- `format.mlx`, `transform.mlx`: the pixel format words and the
  transforms (cmspack.c, cmsxform.c, cmsalpha.c): 8- and 16-bit,
  chunky and planar, swapped, extra channels first or last, reversed,
  byte-swapped 16-bit words, premultiplied alpha, alpha copied.

This directory is the C layer: the header (made by mlx-cheaders from
`api/lcms2.mlx`) and the exported functions over std.colorprofile
(`lcms2.mlx`). Profiles, transforms and tone curves are std.colorprofile's
blocks of the C library's memory; a `cmsCIEXYZ` is three doubles, as the
XYZ tags hold them, so `cmsReadTag` hands those out as they are.

    ./mlx build lcms2          the library and its header
    tools/check_lcms2.sh       builds it, transforms with it and with the
                               system's Little CMS and compares, compiles
                               WebCore's PNG and JPEG decoders

## What it does

- Profiles: `cmsOpenProfileFromMem`, `cmsOpenProfileFromFile` (read only),
  `cmsCreate_sRGBProfile`, `cmsCreateRGBProfile`, `cmsCreateGrayProfile`,
  `cmsCreateXYZProfile`, `cmsCloseProfile`; the header's fields and their
  setters; the tags (`cmsIsTag`, `cmsGetTagCount`, `cmsGetTagSignature`,
  `cmsTagLinkedTo`, `cmsReadTag` of XYZ, curve, `chad` and LUT tags);
  `cmsIsMatrixShaper`, `cmsIsCLUT`, `cmsIsIntentSupported`,
  `cmsDetectRGBProfileGamma`, the formatters of a profile's spaces.
- Tone curves: parametric, gamma, tabulated (16-bit and float), segmented;
  evaluation in floats and 16 bits, reversal, duplication, the table, the
  parameters, linearity, monotony, the gamma estimate.
- Transforms: `cmsCreateTransform` and its THR, multiprofile and extended
  forms, the four ICC intents, black point compensation, the adaptation
  state (1 and 0), `cmsFLAGS_NOOPTIMIZE`, `FORCE_CLUT`, `NOCACHE`,
  `NULLTRANSFORM`, `COPY_ALPHA`, `NONEGATIVES`, the grid point flags;
  `cmsDoTransform`, `cmsDoTransformStride`, `cmsDoTransformLineStride`.
- Colorimetry: D50, xyY <-> XYZ <-> Lab, ΔE, white points of a
  temperature, adaptation to an illuminant.

tests/transform.c prints the same built with gcc against the system's
Little CMS 2.14 as built with mlx-cc against this library, on every pair
of some forty profiles (matrix-shapers v2 and v4 with gammas, tables of 26
to 4096 entries and every parametric type, D50, D65, 5000 K and 9300 K
white points, grey, lut8, lut16, lutAtoB and lutBtoA of XYZ and Lab PCS
with each combination of elements, and damaged ones), for the four
intents and 8- and 16-bit formats: every pixel the same, no tolerance
needed. (Little CMS's own test profiles, its testbed's, give the same
too.)

## What it does not

- Floating-point pixel formats (`TYPE_*_FLT`, `TYPE_*_DBL`), the special
  Lab V2 layouts: the transform is not made (NULL).
- Grids of 2 or of 4 and more inputs (CMYK and multi-ink profiles as
  input): such profiles do not link. Profiles with float tags (D2Bn,
  B2Dn, multiProcessElement) and named colour profiles do not link
  either.
- Intermediate adaptation states of absolute colorimetry (between 0 and
  1), gamut checks, proofing, the black-preserving intents.
- Writing profiles (`cmsSaveProfileTo*`), text tags (`desc`, `cprt` are
  there for `cmsIsTag` but not read), plug-ins, contexts (all are the
  global one), the error handler (kept, never called).
