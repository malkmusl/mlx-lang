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
what is selected, with how many files each holds. A click flies there. Its
switches show or hide the imports, the tree's lines and the names. The
panel lists the connections by kind:

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
    unused [PATH] | unresolved [FILE] | json
```

SYMBOL is `name`, `Container.name`, `path:name` or `FILE:LINE:COL`. The
answers are `path:line:col: kind Qualified.name` lines.

## Checks

`tools/check_codemap.sh` does the following:

1. Validates the shader.
2. Asks the command line.
3. Runs the app under `tools/wayland-test-host` on a small tree, first on
   lavapipe, then on the CPU. In each run it searches, selects, checks the
   connections of what was selected and clicks a group. `MLX_CODEMAP_TRACE`
   has the app print what it read and selected.
4. Compares the pictures from both runs.
