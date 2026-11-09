# Lattice

The engine Voyager moves to: WebKit translated into Mlx, one component at
a time, then grown into an engine of its own (projects/webkit/LATTICE.md
has the plan, the principles and the milestones).

What is here is the translator's output and the tests of it:

- `angle/common/*.mlx`: ANGLE's common/ units as `mlx-cc --emit-mlx`
  writes them (compiler/cc/emitmlx.mlx). Generated files: do not edit by
  hand while the translator is still improving; regenerate with
  `tools/check_lattice_emit.sh --update`. Each file's `// LATTICE:` marks
  name what the translator could not express.
- `angle/common/tests`: the tests of the translated units. ANGLE's own
  unit test as Mlx where it has one (aligned_memory), else a differential
  test: an Mlx driver and a C++ reference (built by mlx-cc against the
  C++ unit) print the same lines for the same inputs.

`tools/check_lattice_emit.sh` builds mlx-cc, translates the units,
compares the output with the files here, compiles them with mlx4 and runs
the tests.
