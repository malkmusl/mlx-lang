# MLX Codemap

The code base as a space to fly through. Every folder is a group with its
files around it, and every file has its declarations around it: functions,
structs, enums, constants, variables and tests. Structs have their fields
and functions around them. Lines show what belongs where and which file
imports which. Select something and new lines show what it connects to:

- cyan for what it calls, uses or imports;
- orange for what calls, uses or imports it.

The panel on the right lists the same connections, with the signature and
the doc comment. It is laid out like the file manager
([`examples/mlx-files`](../mlx-files/main.mlx)): the groups in a
translucent sidebar, back and forward, the name and a search field in the
toolbar, and where you are in the status bar.

```
mlx-codemap [ROOT]          the 3D view of ROOT (default: MLX_CODEMAP_ROOT,
                            else the Git work tree around the working
                            directory; the installed desktop entry opens
                            the repository it was installed from)
mlx-codemap COMMAND ...     the command line (below)
```

## The galaxy

Every use has weight. The index turns each use into a weighted edge,
from the declaration it is in to the declaration it uses; the weight is
how many times. The edges pull on the layout for a few dozen rounds:

- Each edge pulls its two ends toward each other, harder the more uses
  (as a logarithm).
- What pulls on a declaration pulls on its file and on its groups too.
  The pulls are summed up the tree, so pulls inside a group cancel out
  there.
- As a result, a declaration drifts toward the groups that use it, and a
  group drifts toward the groups it depends on.
- A spring holds each node near where it was packed, so the order stays
  readable, and sibling groups are kept from overlapping.

## Findings

The sidebar's *Findings* section opens lists in the panel. The members of
each group are joined by magenta lines in space.

- **Copies:** functions with the same body. First the exact copies, then
  the ones that are the same except for the names of their own locals and
  the values of literals, which one generic function could replace. The
  groups with the most tokens to save come first.
- **Written again:** functions with the same name in several files, that
  is, helpers each file wrote for itself.
- **Unused:** declarations that nothing uses.
- **Workarounds:** where the code works around what the language or its
  compiler does not do yet, grouped by what is missing, the most written
  first (amber). Each place is one more reason to add the feature
  ([`tools/codemap/workarounds.mlx`](../../tools/codemap/workarounds.mlx)):

  | pattern | missing |
  |---|---|
  | `@ptrFromInt(... @intFromPtr(...) ...)` | pointer offsets (`p + n`), slicing a many-pointer |
  | `@ptrCast([*]T, &array)` | `*[N]T` coercing to `[*]T` |
  | `var x: T = undefined` then `unsafe { x = ... }` | unsafe expressions |
  | `const ignored = call(...)`, never read | `_ = value` |
  | `for byte in text { length += 1 }` | a slice's length in the bootstrap library |
  | `aggregates.mark`/`reset`, `turns` | freeing struct literals and structs returned by value |
  | float literals with 12 decimals and more | folding float arithmetic right |
  | `sqrt`, `sin`, `exp`, ... written in Mlx | math builtins |
  | comments: workaround, miscompilation, "does not yet", TODO, FIXME, HACK | what the comment names |

- **Crashes:** what the desktop programs kept when they crashed, newest
  first, each with the function it stopped in and the functions that called
  it (red). The calls are drawn as red lines in space.

Generated code (`/generated/`) is left out.

### Crash flags

The compiler names every function `path:line:name` in the executable's
symbol table. When the file manager, the dock, the compositor or another
desktop program crashes, its crash handler
([`crash.mlx`](../wayland-compositor/crash.mlx)) says which functions the
crash went through and keeps that as a line in
`$XDG_STATE_HOME/mlx/crashes.log` (`~/.local/state/mlx/crashes.log`). The
codemap reads the log ([`tools/codemap/crashes.mlx`](../../tools/codemap/crashes.mlx))
and puts each frame on the function declared at that line. If the file
has changed since the crash, it puts the frame on the function of that
name in the file.

- A declaration a crash stopped in glows red; one a crash went through
  glows faintly.
- Both, and the files and groups that hold them, carry a red flag that
  stays in front of everything, so they can be found from far away.
- The panel says how often it crashed there or on the way, and the last
  crash.

The log is looked at every two seconds, so a crash while the codemap is
open shows up at once, with a note in the status bar.

For the selection, the panel shows:

- how often it is used, and how often from elsewhere (its weight);
- its copies;
- its namesakes;
- the crashes that stopped in it or went through it, and the workarounds
  written in it, with what is missing;
- what is *used much alike*: the declarations that use the same things,
  as a share of everything either one uses.

The command line lists the same findings: `mlx-codemap copies [PATH]`,
`mlx-codemap names [PATH]`, `mlx-codemap unused [PATH]`,
`mlx-codemap workarounds [PATH]` and `mlx-codemap crashes [PATH]`.

## Using it

| | |
|---|---|
| drag | turn the view |
| right drag, Shift+drag | move it |
| wheel, +/- | come closer, go away |
| click | select |
| double click | fly there (on empty space: up a group) |
| typing, Ctrl+F, / | search; Enter or a click flies to a result |
| Escape | clear the search, then the selection, then go up a group |
| Enter | fly to the selection |
| Alt+Up | its group |
| Backspace, Alt+Left / Alt+Right | back / forward |
| arrows | turn |
| Home | everything |
| Ctrl+O, "Open in editor" | open it at its line: `MLX_EDITOR`, else VS Code (`code -g`), else `xdg-open` |
| F5, Ctrl+R, "Read again" | read the code again, keeping the selection |
| Ctrl+W | close |

The sidebar lists the groups, and the groups inside them on the way to
what is selected, with how many files each holds. A click flies there.

The sidebar's check boxes choose what is shown. What is left out is not
drawn, not found by the search, and not counted or listed in the
findings.

- **Groups:** a group's check box leaves it out, or brings it back. A
  dash means something inside it is left out; a click brings all of it
  back.
  - A group inside one that is left out comes back alone. For example,
    clear *Everything*, then tick `std`: only the standard library is
    shown.
  - *Everything* shows all, or (when all is shown) nothing.
  - Then the camera flies to the smallest group holding everything shown
    (`std` in the example). What was selected and is left out is not
    selected any more.
- **Findings:** a finding's check box shows its places in space (Crashes
  is ticked from the start). A click on the finding itself opens its list
  in the panel and ticks it.
- **Workarounds:** the arrow opens the kinds of workarounds, each with a
  check box and how many places it has in what is shown. The list and
  the places in space follow them.
- **Show:** the imports, the tree's lines and the names.
- **Kinds:** functions, structs and unions, enums, constants, variables,
  fields and tests (a struct left out takes its members with it). The
  check boxes are in the kinds' colors.

The groups left out are kept when the code is read again.

The panel lists the connections by kind:

- a declaration: *Calls*, *Uses*, *Called by*, *Used by* and *Members*;
- a file: *Imports*, *Imported by* and *Declarations*;
- a group: *Depends on*, *Used by* and *Contains*. For a group, a
  connection is shown as the group beside it that it goes to. For example,
  `examples/mlx-files` depends on `desktop-shared` and `std`.

A click on a connection flies there. While typing a search, what does not
match fades.

## How it works

- [`tools/codemap/index.mlx`](../../tools/codemap/index.mlx) reads every
  `.mlx` file under the root with the compiler's own lexer and parser. It
  resolves each name to what it declares. For the whole repository (700
  files, 64,000 declarations, 340,000 uses) that takes about 1.5 seconds.
- [`graph.mlx`](graph.mlx) makes the tree of nodes (folders, files,
  declarations, members) and lays it out:
  - a folder's children are packed close, the biggest first, each where the
    whole stays smallest without touching the others;
  - a file's declarations sit on a sphere around it.

  It also works out a node's connections and ranks search matches.
- [`main.mlx`](main.mlx) projects the nodes with an orbit camera
  ([`math3d.mlx`](math3d.mlx): sqrt, sin, cos, ln and exp, written out).
  From that it makes the frame's primitives, and places labels so none
  covers another.
- [`scene.mlx`](scene.mlx) sorts the primitives back to front and bins them
  into 16x16 tiles.
- [`scene_shader.mlx`](scene_shader.mlx) is a compute shader built with
  `std.spirv.builder`. It draws them per tile:
  - lit spheres with a highlight and a rim;
  - soft glows;
  - anti-aliased lines with a gradient.

  Then it puts what the 2D canvas drew (panels, labels) over them. The panel
  (`desktop-shared/panel.mlx`) runs it through its `scene` hook, in the same
  frame after the canvas. Without Vulkan, `scene.drawCpu` draws the same
  picture on the CPU.

The compiler puts struct literals and structs returned by value in an arena
that is never freed. See
[`tools/codemap/aggregates.mlx`](../../tools/codemap/aggregates.mlx): an
index build fills about 80 MB of it. The app takes the arena back after
each event, frame, layout step and build.

## The command line

The same binary with a command, or `tools/codemap/main.mlx` built on its
own:

```
mlx-codemap [-C ROOT] stats | files [TEXT] | outline FILE | find TEXT |
    show SYMBOL | def FILE:LINE:COL | refs SYMBOL | callers SYMBOL |
    callees SYMBOL | imports FILE | importers FILE | members SYMBOL |
    unused [PATH] | copies [PATH] | names [PATH] | workarounds [PATH] |
    crashes [PATH] | unresolved [FILE] | json
```

SYMBOL is `name`, `Container.name`, `path:name` or `FILE:LINE:COL`. The
answers are `path:line:col: kind Qualified.name` lines.

## Checks

`tools/check_codemap.sh` does the following:

1. Validates the shader.
2. Asks the command line, also for the workarounds and a kept crash.
3. Runs the app under `tools/wayland-test-host` on a small tree, first on
   lavapipe, then on the CPU. In each run it searches, selects, checks the
   connections of what was selected, clicks a group and then its check box.
   `MLX_CODEMAP_TRACE`
   has the app print what it read and selected, the crashes it put on
   declarations, and how much each finding shows.
4. Compares the pictures from both runs.

`tools/check_crash_report.sh` checks the crash reports themselves: the
symbol table, the functions a crash names and the kept log.
