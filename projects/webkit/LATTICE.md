# Lattice

Lattice is the engine Voyager moves to after the WebKit port's bootstrap
(PORT.md's milestones): WebKit translated into Mlx, then grown into an
engine of its own, as Blink grew out of WebKit. It starts as WebKit 2.54.1
(projects/webkit/upstream, kept exactly as released: UPSTREAM.md,
tools/check_webkit_upstream.sh) and is built, like everything here, only
with this repository's toolchain.

Voyager names its engine in one place, `ENGINE` in
port/platform/mlx/UserAgentMlx.cpp: today `Voyager/0.1 (WebKit 2.54.1;
Wayland)`, with Lattice in a shipping build `Voyager/0.x (Lattice 0.0.1;
Wayland)`. The compatibility part of the user agent (`AppleWebKit/605.1.15
... Safari/605.1.15`) stays as long as Lattice behaves like WebKit, as
Blink kept `AppleWebKit/537.36`.

## The principles

- **Translate first, change later.** A component is first translated so
  that it does what the C++ does, line for line where that is sensible,
  with Mlx's means where they fit (std containers, std.heap, `@noncopy`
  with `deinit`, tagged optionals, errors instead of crashes). Redesign
  and optimisation come once a component is at parity, measured with
  mlx-profile, never in the same step.
- **The C++ is the reference.** projects/webkit/upstream is never
  edited. Every translated component is tested against the C++ one it
  replaces, on the same inputs (differential tests), and the browser's
  checks (layout, paint, storage, network, crypto, compositing,
  check_voyager.sh) pass the same with either.
- **One component at a time, in the running browser.** A component
  replaces its C++ counterpart behind the interface the two share, so
  Voyager runs with any mix of translated and C++ components. A build
  switch selects them (`LATTICE=translator,...`); the default moves to
  the translated one once it is at parity, and the C++ one is retired
  from the build (PORT.md's "What is retired").
- **Along narrow interfaces.** Mlx calls C and is called as C (an
  `export fn` and its header from mlx-cheaders); it does not call C++
  templates. A component is therefore cut out where its interface is
  C-like or can be made so by a thin C++ shim of the port's own: ANGLE's
  EGL/GLES API, the shader translator's `sh::` entry points, the port's
  clients and strategies, Skia's canvas. Large cores whose insides are
  templates throughout (WTF, JavaScriptCore, WebCore) are translated as
  whole layers, not function by function; how is decided with the
  experience of the pilot below.
- **The code map knows both sides.** mlx-codemap links an Mlx `export fn`
  with its C++ callers; the index of what is compiled (C++ and Mlx) is
  the map of what is left to translate (`mlx-codemap reach`, `refs`).

## The tool

Translation is not done by hand from the text. mlx-cc already parses and
type-checks all of WebKit (templates instantiated, overloads resolved,
names bound); a translator built on its front end (`mlx-cc --emit-mlx`, a
step of L0) writes a first Mlx version of a component from that typed
tree: declarations, control flow, expressions with their resolved types,
instantiated templates as generic functions or concrete copies. The hand
then makes it Mlx (ownership, errors, containers), and the differential
tests say when it is right. What the translator cannot express is listed
per component, so the tool improves before the hand work grows.

### The translator today (L0)

`mlx-cc --emit-mlx=FILE unit.cpp` (compiler/cc/emitmlx.mlx) writes the
definitions of a unit's main file -- its structs, globals and functions,
not what its headers declare -- as Mlx from mlx-cc's typed tree, after the
unit is parsed and type-checked with the same flags the build uses. What
it does now:

- declarations: structs as `extern struct`s with the C++ layout (each
  run of bit-fields a `packed struct` word of 1, 2, 4 or 8 bytes at the
  bytes C++ gives it, `bits8: Record_bits8`; an unnamed member -- an
  anonymous union, a table pointer -- `fieldN` by its byte in the
  object), module constants and variables with their initial values
  (arrays of scalars as `[N]T{ ... }` literals, the tables of
  Float16ToFloat32; tables of records, a base class's members in place;
  addresses of functions, strings and other variables),
  the headers' constants the functions name, functions with Mlx types
  for the C++ ones (`?[*]T` for pointers, `u64`, `f32`, a plain `char`
  as `u8`), C++ overloads told apart by their parameter types in the
  name, the C library's functions the unit calls (`malloc`, `memcpy`,
  `strlen`, `strtod`, `fputs`, `operator new`...) as calls on a runtime
  module written in Mlx (projects/lattice/runtime/c.mlx: no C library
  under the translation), anything else called but not defined as an
  `extern("c")` declaration;
- statements: blocks, `if`, `while`/`for` as `while` with labelled
  `break` and `continue` (a `for` loop's increment runs before each
  `continue`; a condition that hoists statements, `i-- > 0`, becomes a
  `while true` with the condition tested first), `switch` as `match`
  (grouped case labels as ranges, the body repeated under the labels a
  range cannot join; a return ends the arm; the `default` arm written
  last, as Mlx tries the arms in order; `break` and `continue` pass
  through the arms to the loop around), `do { } while (0)` as its body,
  returns (and `return undefined` after a body that ends in a `for (;;)`
  with the returns inside); a comma expression's temporaries hoisted as
  statements before the one they serve; only the locals the Mlx uses are
  declared, a value-initializing zero before an initializer dropped;
- expressions with their resolved types: casts written as
  `@intCast`/`@floatCast`/`@floatFromInt`/`@intFromFloat`/`@bitCast`,
  `@ptrCast` and `@ptrFromInt` in `unsafe`, `*(p + i)` and a decayed
  array's `a + i` back to `p[i]` with a usize index (through a many-item
  view when `p` points at records: Mlx's `*S` does not index), `p - n`
  as `&p[i - n]`, a C pointer unwrapped (`.?`) where the callee takes
  `this` or a reference or a reference variable is bound, `std::min`/`max`
  (two arguments or a braced list) inline, `floor`, `ceil`, `sqrt`,
  `pow` and `log` on std.math, `std::numeric_limits<T>::max()`/`min()`
  as the number, `gl::bitCast` as `@bitCast`, string literals (`"gl_"`
  where a `const char *` is wanted as `"gl_".pointer`);
- ANGLE's `ASSERT`/`ERR` stream machinery dropped with a
  `// LATTICE: a log dropped:` mark carrying the message's strings.

The headers come along where the file needs them: a record a header
defines (ANGLE's own, complete, without virtual functions, its members
of types the Mlx has) is written as a struct too, a base class's members
in place, with its inline methods and template instances (`Mat4`,
`IndexRange`, `Vector<3, float>` with `VectorBase<3, float>`, `BitSetT`,
`MemoryBuffer`); so are the headers' inline and template functions the
file calls (`Bit`, `ScanReverse`, `std::copy`, `std::exchange`,
`std::swap`) and the constants it names. A method is `Class_method(self:
*Class, ...)`, `*const Class` when the C++ method is const, a constructor
`Class_init`, a destructor `Class_deinit`, an operator named (`eq`,
`add`, `mul`, `at`); a function returning a class through C++'s hidden
buffer assigns the buffer's variable; a by-value parameter the body
changes or takes the address of is copied into a `var`. `std::array<T,
N>` is `[N]T` with `data()`, `size()` and `[]` inline; `std::move` and
`std::forward` are their argument; `std::numeric_limits<T>::max()` is the
number; the C library's and `std::` math functions are std.math and the
float builtins.

Headers resolve to modules, so the output is pure Mlx with no C or C++
under it. `--emit-mlx-units=LIST` names the units translated alongside:
a function or record a header declares whose definition is another
unit's (utilities.cpp calling string_utils.cpp's `BeginsWith`,
`IndexRange` from the header of another unit) is `unit.name` on
`const unit = @import("./unit.mlx")`, not an extern. `--emit-mlx-std=
IMPORT` names the std module: mlxlibcpp's string.cpp translated by the
same translator (projects/lattice/runtime/libcpp.mlx, 228 functions,
no externs; the file's explicit instantiations say which of the
header's templates it exports), so `std::string`, `std::string_view`
and `char_traits<char>` are `libcpp.string`, `libcpp.basic_string_view_
...` with every method (`libcpp.string_find_const__const_string_u64`)
-- the std headers `string` and `string_view` belong to that module, the
other std headers' template instances (`std::vector<T>`, `std::
allocator<T>`) come along with each unit that uses them. Overloads are
named by their parameter types without the module, so the name is the
same inside the module and in the units that import it. Every inline
body is read when translating (not only the ones a unit calls), so a
header's record comes along with all its methods. The C library is the
runtime module: `c.malloc`, `c.strtod`, `c.stdout()`, `c.fputs` and the
rest in Mlx over std.heap and std.io (projects/lattice/runtime/c.mlx),
`--emit-mlx-runtime=IMPORT` says where it is.

Classes with virtual functions are records like the others: the table
pointer is a member (`field0`, a second base's `field16`), the vtable a
module-level `var` of function addresses (static data: one address,
which every object of the class points at), a virtual call the function
read from the object's table at its slot and called with the adjusted
`this`, as the front end lowered it; `delete p` through a base calls
the class's deleting destructor (`Class_deinit_deleting`, written from
the destructor and `operator delete`). A base class that is not the
object's start is addressed where it lies; a class made from or
assigned through its base writes the base's bytes. A class with a copy
constructor taken by value is taken by value in the Mlx (C++ hands over
the address of a copy the caller made). The overflow builtins
(`__builtin_mul_overflow`) are calls of the runtime
(`c.mul_overflow_u64`), `std::atomic<T>` the atomic builtins, bit
counting `@popCount` and the runtime's `clz`/`ctz`.

What C++ has and Mlx has not is written out of what Mlx has:

- **Variadic functions** take their extra arguments as words,
  `va_arguments: []const u64` (an integer extended, a pointer's
  address, a double's bits); a call packs them, `[_]u64{ a, b }[0..2]`.
  `va_list` is the runtime's record (`c.VaList`, laid out as the System
  V one, the words its overflow area), `va_start`, `va_arg` and
  `va_copy` its functions. The C library's `printf` family, `sscanf`,
  `syscall` and `fcntl` are the runtime's, over mlxlibc's format engine
  (itself Mlx).
- **`goto`, and a `switch` whose cases fall through**, are a dispatcher:
  the statements between the labels are numbered states of a labelled
  `while true`, a jump sets the state and continues it, falling through
  is the next state (flex's and bison's tables in the shader translator
  run so). A loop a jump leaves gets its label when the jump is written.
- **Objects made before `main`** (a class object with a constructor, a
  variable whose initializer is no constant) are constructed in the
  unit's `pub fn lattice_start()`, which the program's `start()` calls
  for every unit, the library's first; constant objects are static data
  (bit-fields in their words, addresses of functions, of other objects
  and into arrays, a vtable's address point). A static local is behind
  its guard (`c.__mlxcc_guard_acquire`).
- **A pointer to a member function** is a word, the function's address
  or, for a virtual one, its table offset and one (functions begin at
  multiples of 16, so the low bit tells them apart); the call reads the
  table when the bit is set.
- **What one side of `?:`, `&&` or `||` hoists** (a temporary
  constructed for one arm) runs on that side's condition: the condition
  is kept in a variable of the function (`chosen_N`), the hoisted
  statements go under an `if` of it, and the expression reads it.
- **Operators outside a class** are functions like any other
  (`angle_base_internal_add`); an object of a class copied by its bytes
  that is assigned a value of another type is assigned the temporary its
  converting constructor makes.
- **Names** the Mlx would confuse are told apart: a member that hides a
  base's (`name_2`), a record or C++ function beside a C function of
  its name (`stat_`), a local named as a class the module writes
  (`string_3`), two records of one name in two units, unnamed globals
  (`anonymous_2`).

`MLX_CC_EMIT_TRACE=1` writes every expression statement under a comment
with its tree as the front end handed it over (node kinds, variables'
names, type kinds): where a line of the Mlx is not what the C++ says,
the tree shows whether the translator or the front end is wrong.

What is left to the hand is marked `// LATTICE:` in place and counted at
the end of the file: a function taking or returning a type the Mlx lacks
by value (written out, commented, until the type has an Mlx shape -- a
pointer to such a type is passed along, the type declared opaque), one
calling such a function (or one held back so), `alloca`, inline
assembly (cpuid, the time stamp), ANGLE's log streams. Nothing is
made up: what the translator cannot write is a comment, never a `0`. The output is kept in the repository
(projects/lattice/angle/common and projects/lattice/runtime/libcpp.mlx),
regenerated with `tools/check_lattice_emit.sh --update`, and the check
fails when the translator's output drifts from what is kept. The pilot
units: ANGLE's aligned_memory.cpp (its unit test as Mlx), mathutil.cpp
(RGB9E5 packing), Float16ToFloat32.cpp (every half-precision pattern),
utilities.cpp (the GL type and target predicates, sizes, counts and
orders over the whole enum range), matrix_utils.cpp (Mat4 made, rotated,
translated, scaled, frustum, perspective, ortho, products),
MemoryBuffer.cpp (a buffer resized, reserved, filled, cleared,
destroyed), platform_helpers.cpp and string_utils.cpp (splitting,
trimming, prefixes, affixes, tokens, case, replacement and wildcards,
through the std module's string and string_view); each differential test
prints the same, bit for bit, from the Mlx and from the C++ built by
mlx-cc and run on mlxlibc (whose libm wraps the same std.math). Along
the way mlx4 learnt packed structs laid out in bits
(tests/434_packed_struct_runtime.mlx: C++ bit-fields are everywhere in
ANGLE), to negate a float by its sign bit
(tests/435_float_negative_zero_runtime.mlx: Mat4::Ortho's `-0.0`),
function types where a builtin takes a type (436: a virtual call's
`@ptrCast(fn(...) -> T, slot)`), atomics on optional pointers (437:
`std::atomic<T *>`), and module-level variables initialized with floats
and addresses (438: a vtable is static data at one address; a `const`
aggregate is a value built where it is used).

Beside the units, five feature tests translate a small C++ file each and
compare its result with the C++'s (projects/lattice/tests): `virtual`
(overriding, a pure virtual, deletion through the base, a second base),
`bits` (bit-fields beside other members, an anonymous union, two bases
with table pointers), `idioms` (pointers stepped through members,
`std::array` members, static locals of one name, a class beside a
function of its name, references from pointers, an object in bytes of
storage, an atomic pointer), `values` (a class with a copy constructor
by value, an empty base, a base assigned through the derived object) and
`overflow` (the overflow builtins at the edges of each type).

### One program of many units

A unit translated alone carries along what its headers define, so a
record, an inline function or a template's instance is written in every
unit that uses it, a type of its own in each, and a call into another
unit is an `extern` declaration. For one program each such thing has one
home (compiler/cc/emitmlx.mlx, "One program of many units"), in three
steps that `tools/lattice_translate.sh` runs over ANGLE and the C++
library's own units (compiler/libcpp):

1. `mlx-cc --emit-mlx-unit=NAME --emit-mlx-index=FILE`: the unit is
   translated and lists every definition it writes: a record (in full or
   opaque), a variable, a function with the name it would have alone and
   the name with its parameter types, whether it is held back on its own
   account, the functions it calls, and what it waits for.
2. `mlx-cc --lattice-manifest=OUT --lattice-units=LIST`
   (compiler/cc/lattice.mlx): a thing's home is the first unit that
   defines it (a record's the first that writes it in full); its name is
   unique in its home (a function's carries its parameter types when
   another function of the home has the name); a function is held back
   when one it calls is, in whatever unit -- a fixed point over the
   calls of the whole program.
3. `mlx-cc --emit-mlx-unit=NAME --emit-mlx-manifest=FILE`: the unit is
   translated again. It walks what it walked before; a definition whose
   home is another unit is dropped, names are the manifest's, and what
   lives elsewhere is named through its home's module
   (`src__common__string_utils.angle_BeginsWith(...)`,
   `?*src__libANGLE__Context.gl_Context`). A module imports the modules
   it names; they import each other freely.

A thing's key says what it is across the units: a function's and a
variable's C++ symbol (with the unit before it when it is the unit's
own: `static`, an anonymous namespace), a record's C++ name. In such a
program the library's classes are records like any other -- `std::mutex`,
the tree under `std::map` and `std::set`, `shared_ptr`'s control block,
`std::function`, a lambda's closure -- and the library's own units
(`libcpp/string.cpp`, `iostream.cpp`) are units of the program: the std
module of a unit translated alone is not needed. `tools/lattice_why.sh
DIR` ranks what the functions held back wait for.

### Where ANGLE stands

`tools/lattice_translate.sh --compile` translates every unit the build
compiles of ANGLE (projects/webkit/lists/angle*.txt: 337 units, 397,199
lines of C++ without the headers) and the C++ library's four units
(compiler/libcpp) as one Mlx program, and gives it to mlx4:

| | |
|---|---|
| modules | 341, 66.3 MB of Mlx, each thing once, in its home |
| functions written | 38,162 |
| functions held back (written as comments) | 0 |
| what mlx4 says | the program compiles: every function passes the analysis, the lowering and the code generator; an executable of 28.9 MB in 19 s |
| functions declared `extern` | 302 by name |
| `// LATTICE:` marks in live code | 124 objects zeroed through an address, 51 initializers not read, 10 floating constants, 7 unknown nodes, 4 `std::min` arguments (the other marks are comments: a base class's name, a C++ symbol) |

Three levels, and where each is:

1. **Proven** (the differential test prints the same from the Mlx and
   the C++): eight units of `src/common`, 3,091 lines, and the feature
   and program tests of the translator itself (projects/lattice/tests,
   twenty-two of them: virtual calls, bit-fields, variadics, jumps,
   objects made before main, two units as one program, the shapes the
   whole program showed).
2. **Compiles**: all of ANGLE, as one program. This is what the work
   of the last rounds was: 7,154 functions were held back when the
   units were translated alone; with every unit translated together
   none is, and mlx4 accepts all of them.
3. **Runs**: not yet. The executable asks its loader for the 302
   functions it declares `extern`, and they are the rest of the way to
   an ANGLE without C or C++ under it:
   - 153 Vulkan functions (`vk*`) and 20 of the memory allocator
     (`vma_*`): std.vulkan has Vulkan in Mlx; the translation names the
     C functions still.
   - 100 or so of the C library (`pthread_*`, `dlopen`, `fopen`,
     `getenv`, `stat`, the float math functions `sinf`...): the runtime
     module (projects/lattice/runtime/c.mlx) has the first sixty over
     std; mlxlibc has all of them in Mlx.
   - 9 of zlib (`deflate`, `inflate`, `crc32_z`) and 6 of xxHash: std
     has both.
   - 20 functions of the C++ library declared with their C++ symbols
     (`std::array<T, N>::fill`): header functions the translator did
     not carry along.

What mlx4 and mlx-cc learnt on the way (each with its test): record
types across modules that import one another (441), a function ended by
an endless loop (440), functions at multiples of 16 (442), function
types with function types among their parameters (443), a statement
ended by a line's last `.*` or `.?` (444), the address of a module-level
constant (445), an arena of 8 GiB of address space; in mlx-cc, an
assignment through a converting constructor of a class copied by its
bytes, and `auto &row` over an array of arrays.

The order of what is next: the 302 externs (the C library through the
runtime, Vulkan through std.vulkan, zlib and xxHash through std, the 20
library functions carried along), so that the program starts; the
shader translator run against the C++ one shader by shader; the marks
in live code; the EGL and GLES entry points as `export fn`, which is
what WebKit links.

## The pilot: ANGLE

ANGLE (Source/ThirdParty/ANGLE) comes first: it is self-contained, its
interfaces are C (EGL, GLES) and a small C++ API (the shader
translator's), it is needed for WebGL (PORT.md, M8 part 2), and
std.vulkan and std.spirv exist already. What WebGL needs of it, in lines
of C++ (2.54.1):

| Part | Lines | Role |
|---|---|---|
| `src/common` | ~91,000 | utilities, math, hashing, the platform layer |
| `src/compiler/translator` | ~161,000 | GLSL ES → SPIR-V (the parts the Vulkan backend uses) |
| `src/libANGLE/renderer/vulkan` | ~170,000 | the Vulkan backend |
| rest of `src/libANGLE` | ~200,000 | GL state, validation, the front end (without the backends WebGL does not use) |
| `src/libGLESv2` | ~115,000 | the entry points, mostly generated |

The order:

1. **The shader translator** (`projects/lattice/angle/translator`): GLSL
   ES 1.00/3.00 to SPIR-V on std.spirv, behind the `sh::` API the C++
   one has (a C shim exports it to ANGLE's C++). Differential tests: every
   shader of the WebGL LayoutTests and the conformance tests run, through
   both translators: the same compile errors, the same uniforms and
   attributes reported, and SPIR-V that validates and renders the same
   pixels on lavapipe.
2. **The Vulkan backend** on std.vulkan directly (no C Vulkan API in
   between once it is Mlx): the paint and WebGL checks the same.
3. **GL state, validation and the entry points**: libmlxangle with the
   EGL/GLES C API WebCore's GraphicsContextGLANGLE calls, then the C++
   ANGLE retired.

## Milestones

| | Goal | Done when |
|---|---|---|
| L0 | Groundwork | WebKit in the repository unmodified and guarded (done); the build takes it from there (done); the code map indexes everything compiled; `mlx-cc --emit-mlx` translates a sample of ANGLE's common/ that compiles with mlx4 and passes ANGLE's own tests of it (done: eight units and mlxlibcpp's string as the std module, pure Mlx on a runtime module in Mlx, tools/check_lattice_emit.sh) |
| L1 | ANGLE's shader translator in Mlx | the differential tests above pass for every shader of the WebGL tests; Voyager runs WebGL with it |
| L2 | ANGLE in Mlx | WebGL checks and paint pages the same with the Mlx ANGLE; the C++ ANGLE retired from the build |
| L3 | The plan for WebKit | from L0–L2's experience: the order of WTF, JavaScriptCore, WebCore and the port, what is translated and what is rewritten, written here |
| L4 and on | WebKit, layer by layer | each layer as ANGLE was: at parity, then the default, then the C++ retired; `ENGINE` becomes Lattice with the first layer in a shipping build |

## After parity: cleaning up

Once Lattice runs on its own, the translated code is untangled, layer by
layer, with the code map as the guide: `mlx-codemap complex` (the
functions to split), `copies` (code that exists twice), `unused`,
`cycles` and `layers` (imports that cross the layers codemap.layers
sets), `workarounds` (hand-written tables and allocators), `hot` (where
the time goes). Each cleanup is its own change with the browser's checks
passing before and after, so behaviour stays what the parity tests
proved; what WebKit's history left behind (ports, platforms and switches
Lattice does not have) goes first.

## Open questions

- How templates become Mlx: generic functions with `comptime` types
  where the C++ is generic in earnest, concrete copies where it is
  instantiated a few times.
- Where the translator's output is kept: generated files in the
  repository (reviewable, edited by hand afterwards) versus a build step;
  the first, as hand work follows.
- Threads and memory: WTF's RefPtr and the garbage collector's rules
  versus Mlx's ownership; settled with JavaScriptCore at L3, not before.
