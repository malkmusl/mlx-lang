# MLX Observatory IDE

An IDE for Mlx around a map of the code: fly through the code base,
open what you find in the built-in editor, see the compiler's problems,
the crashes, the workarounds and the time spent right on the lines, then
build the program and commit, without leaving the window. *Observatory*
for short.

It knows more of the code than an editor usually does: every use of every
name, which programs and checks reach a function, what it became in
machine code, how often it crashed and where, how much of the time it
takes, who changed it, its copies and what the language is missing in
it. The editor shows all of that where the code is (see
[The editor](#the-editor)).

The map is the code base as a space to fly through. Every folder is a group with its
files around it, and every file has its declarations around it: functions,
structs, enums, constants, variables and tests. Structs have their fields
and functions around them. Lines show what belongs where and which file
imports which. Select something and new lines show what it connects to:

- cyan for what it calls, uses or imports;
- orange for what calls, uses or imports it.

The panel on the right lists the same connections, with the signature and
the doc comment, and has back and forward at its top. It is laid out like
the file manager ([`examples/mlx-files`](../mlx-files/main.mlx)): the
groups in a translucent sidebar, the name, Build, the history (*Since
HEAD*, with Commit) and a search field in the toolbar, and where you are
in the status bar. There is nothing to read again by hand: files changed
on disk (by Git, another editor or a save) are read again by themselves.

At the top of the sidebar, *View* switches between the **Map** and the
**Editor** (Ctrl+E); see [The editor](#the-editor) and
[Build and commit](#build-and-commit).

![A flight through this repository: the galaxy turns, a search finds
drawSidebar, the camera flies to it and to its group, the spheres take
the colors of where the time goes, then the crashes, and the hottest
function opens in the editor.](screenshots/tour.gif)

All pictures here are of this whole repository, with real data: the
crashes of [`tests/support/crash_report.mlx`](../../tests/support/crash_report.mlx),
profiles of `mlx-codemap` itself under `mlx-profile`, the programs built,
and the Git history. The editor's pictures edit a real file and put it
back. [`tools/observatory_screenshots.sh`](../../tools/observatory_screenshots.sh)
takes them again (see [Pictures](#pictures)).

```
mlx-observatory [ROOT]      the 3D view of ROOT (default: MLX_CODEMAP_ROOT,
                            else the Git work tree around the working
                            directory; the installed desktop entry opens
                            the repository it was installed from)
    --only GROUP,GROUP      only these groups shown (paths: std,
                            examples/mlx-files)
    --view NAME             a view of codemap.views in the search
    --query WORDS           a query in the search
    --diff REV              the base of the finding Changed (HEAD)
mlx-observatory COMMAND ... the command line (below; installed also as
                            mlx-codemap)
```

## Screenshots

<table>
<tr>
<td width="50%"><img src="screenshots/galaxy.png" alt="The whole repository as a galaxy"><br>
<b>The galaxy.</b> Every top folder a group, the files around it, the
declarations around them; the sidebar lists the groups and the findings.</td>
<td width="50%"><img src="screenshots/search.png" alt="Searching drawSidebar"><br>
<b>Search.</b> Typing finds declarations by name, with where they are.</td>
</tr>
<tr>
<td><img src="screenshots/selected.png" alt="drawSidebar selected"><br>
<b>A declaration selected.</b> Cyan lines to what it calls and uses, the
panel with its signature, doc comment, machine code, commits,
complexity, risk and workarounds.</td>
<td><img src="screenshots/group.png" alt="The group mlx-codemap selected"><br>
<b>A group selected.</b> What it depends on and what uses it, the files
it holds.</td>
</tr>
<tr>
<td><img src="screenshots/only-std.png" alt="Only std shown"><br>
<b>Check boxes.</b> Only <code>std</code> is shown; the findings count
what is shown.</td>
<td><img src="screenshots/program-machine-code.png" alt="The program mlx-codemap, colored by machine code"><br>
<b>A program.</b> Only what <code>mlx-codemap</code> reaches from its
<code>main</code>, colored by the machine code it became.</td>
</tr>
<tr>
<td><img src="screenshots/churn-complex.png" alt="Colored by changes, the complex functions"><br>
<b>Changes and complexity.</b> Colored by the commits that wrote each
declaration (Git blame); the complex functions listed.</td>
<td><img src="screenshots/risk.png" alt="Colored by risk, the risky functions"><br>
<b>Risk.</b> Crashes, workarounds, commits, complexity, copies and
missing checks together.</td>
</tr>
<tr>
<td><img src="screenshots/heat.png" alt="Colored by run time, the hot functions"><br>
<b>Hot.</b> Where <code>mlx-codemap</code> spent its time under
<code>mlx-profile</code>.</td>
<td><img src="screenshots/crashes.png" alt="The crashes kept"><br>
<b>Crashes.</b> Each kept crash with the line it stopped on and the
functions that called it; red flags in space.</td>
</tr>
<tr>
<td><img src="screenshots/workarounds.png" alt="The workarounds, with their course over the last commits"><br>
<b>Workarounds.</b> By what the language is missing, with their course
over the last 20 commits (the sidebar's small bars, the panel's chart).</td>
<td><img src="screenshots/rewrite.png" alt="The rewrite of dropped results"><br>
<b>Rewrite.</b> Every <code>const ignored = f()</code> as it would become
<code>_ = f()</code>; <i>Apply</i> writes them.</td>
</tr>
<tr>
<td><img src="screenshots/copies.png" alt="Copies"><br>
<b>Copies.</b> Functions with the same body, the most to gain first.</td>
<td><img src="screenshots/untested.png" alt="Untested functions"><br>
<b>Untested.</b> What a program reaches and no check does.</td>
</tr>
<tr>
<td><img src="screenshots/structure.png" alt="Cycles and layers"><br>
<b>Cycles and layers.</b> Files that import each other round about, and
imports the rules forbid.</td>
<td><img src="screenshots/base.png" alt="Choosing the base of Changed"><br>
<b>Since ...</b> The base Changed compares with: not committed yet, or one
of the last commits.</td>
</tr>
<tr>
<td><img src="screenshots/changed.png" alt="Changed since HEAD~3"><br>
<b>Changed.</b> The declarations added and changed since that commit
(and, at the end of the list, the ones removed).</td>
<td><img src="screenshots/filter.png" alt="The query builder"><br>
<b>Filter.</b> Adds query words to the search, first what fits the
selection.</td>
</tr>
<tr>
<td><img src="screenshots/query.png" alt="A query in the search"><br>
<b>A query.</b> <code>kind:fn in:std/ uses:Allocator -untested</code>:
what does not answer is left out; the field counts what does.</td>
<td><img src="screenshots/views.png" alt="Views, with sections folded up"><br>
<b>Views.</b> Queries kept in <code>codemap.views</code>, at the end of
the sidebar; every section folds up.</td>
</tr>
<tr>
<td><img src="screenshots/editor.png" alt="scanFile in the editor"><br>
<b>The editor.</b> The hottest function of the profiles: the heat bar by
the line numbers, the lens row above it (uses, time, risk, complexity,
machine code, commits), names in the colors of what they are
(parameters, fields, types, functions), the uses of the name at the
cursor lit.</td>
<td><img src="screenshots/editor-hover.png" alt="Hovering a field"><br>
<b>Hover.</b> What a name is and where it belongs: the field
<code>index</code> of the struct <code>Workarounds</code>, its type
<code>*Index</code> and where that is declared.</td>
</tr>
<tr>
<td><img src="screenshots/hover-doc.png" alt="Hovering a documented function"><br>
<b>Hover with its doc.</b> As rust-analyzer shows it: where it is, the
signature in the code's colors, the doc comment set as text, what the map
measures.</td>
<td><img src="screenshots/hover-struct.png" alt="Hovering a struct"><br>
<b>A struct.</b> Its fields under it, each with its type.</td>
</tr>
<tr>
<td><img src="screenshots/completion-std.png" alt="Completion in std.fs"><br>
<b>std.fs.op...</b> The members of a module after its dot, the best fit
first; the chosen one's place, signature and doc at the side.</td>
<td><img src="screenshots/completion.png" alt="Completion after a dot"><br>
<b>Completion.</b> The members of <code>*Workarounds</code> after the
dot, each with its type and where that is.</td>
</tr>
<tr>
<td><img src="screenshots/signature.png" alt="Signature help"><br>
<b>Signature help.</b> In a call, the function's signature, the argument
the cursor is at lit.</td>
<td><img src="screenshots/references.png" alt="The uses of placeAt"><br>
<b>References.</b> Shift+F12 or the lens's uses: every use, with its
line; Enter opens one.</td>
</tr>
<tr>
<td><img src="screenshots/rename.png" alt="Renaming a field"><br>
<b>Rename.</b> F2: every use the index knows gets the new name, in every
file.</td>
<td><img src="screenshots/blame.png" alt="The blame of a line"><br>
<b>Blame.</b> The pointer on a line number: the commit that wrote the
line, who and when.</td>
</tr>
<tr>
<td><img src="screenshots/workaround-hint.png" alt="A dropped result in std"><br>
<b>Workarounds on their line.</b> What it works around and what the
language is missing; <i>Rewrite</i> for the kinds that can be.</td>
<td><img src="screenshots/editor-problem.png" alt="A compiler error on its line"><br>
<b>The compiler's problems.</b> Saved with a name nobody declared: the
compiler's errors on their lines, the changed line marked, the count and
the message at the cursor in the status bar.</td>
</tr>
<tr>
<td><img src="screenshots/commit.png" alt="The history with the commit"><br>
<b>Commit.</b> In the history (<i>Since HEAD</i>, Ctrl+K): the changed
files and a message; Enter commits, <i>Commit and push</i> pushes too;
below, the commits Changed can compare with.</td>
<td><img src="screenshots/build.png" alt="Built"><br>
<b>Build.</b> The program that reaches the file shown, into
<code>mlx-out/bin</code>, in the background.</td>
</tr>
<tr>
<td><img src="screenshots/map-after-editor.png" alt="Back in the map"><br>
<b>Back to the map.</b> The map has read the saved files again and stands
on the declaration the cursor was on.</td>
</tr>
</table>

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

  Two kinds can be rewritten away, now that the compiler does without
  them: dropped results (`const ignored = call(...)` becomes `_ =
  call(...)`) and counted lengths (`for byte in text { length += 1 }`
  becomes `length += text.length`)
  ([`tools/codemap/rewrite.mlx`](../../tools/codemap/rewrite.mlx)). In the
  panel, *Rewrite ...* under the kind shows each line before and after;
  *Apply* writes them and reads the code again. It can only when the
  compiler (`$MLX_COMPILER`, else `mlx-out/bin/compiler/mlx4`, else
  `mlx4`) builds a program with what they need. On the command line:
  `mlx-codemap rewrite dropped|counted [PATH] [--apply]`. Files under `compiler/`,
  `std/bootstrap/` (the bootstrap compiler builds them) and `tests/` are
  left as they are.

  The panel shows how many workarounds the last 20 commits had, a bar
  each; *Count the history* counts the commits not counted yet in the
  background ([`tools/codemap/trend.mlx`](../../tools/codemap/trend.mlx)).
  Each commit is taken out with `git archive` and counted once; the counts
  are kept in `~/.cache/mlx/codemap/workarounds-v1`. `mlx-codemap history
  [N]` does the same on the command line, with a row of bars per kind.

- **Crashes:** what the desktop programs kept when they crashed, newest
  first, each with the function it stopped in and the functions that called
  it (red). The calls are drawn as red lines in space.

- **Unreachable:** functions no program and no check reaches from its
  `main`, and that are in no binary: dead code, however often other dead
  code uses it. Biggest first.
- **Untested:** functions a program reaches and no check does. The most
  complex first.
- **Complex:** functions with 15 branches or more, blocks 6 deep, or 1500
  tokens ([`complexity.mlx`](../../tools/codemap/complexity.mlx)).
- **Cycles and layers:** files that import each other round about, and
  imports the rules in `codemap.layers` forbid
  ([`structure.mlx`](../../tools/codemap/structure.mlx)); drawn as blue
  lines in space.
- **Risky:** functions most likely to break. Crashes, workarounds,
  commits, complexity, copies and missing checks together
  ([`measures.mlx`](../../tools/codemap/measures.mlx)).
- **Hot:** where the programs spend their time, from the profiles of
  `mlx-profile`. With a built program chosen under Program, *Profile* in
  the panel runs it under `mlx-profile` (next to `mlx-codemap`, else on
  the `PATH`); when it ends, its profile is read.
- **Changed:** declarations added and changed since a commit, mint
  green. The base is chosen in the toolbar (*Since HEAD*: what is not
  committed yet, or one of the last commits) or with `--diff REV`. The
  files Git names as changed are taken out of that commit and indexed on
  their own; declarations are matched by file, containers, name and kind
  and compared by their text without blanks
  ([`diff.mlx`](../../tools/codemap/diff.mlx)). The list ends with the
  declarations removed. `mlx-codemap diff [BASE] [OTHER]` prints the same
  (`+` added, `~` changed, `-` removed), also between two commits.

Generated code (`/generated/`) is left out.

### Programs

Every file with a `main` (outside the checks) is a program: the coreutils,
the desktop programs, the tools, the compiler
([`programs.mlx`](../../tools/codemap/programs.mlx)). What its `main` and
its exported functions use, and what that uses, is what the program is
made of. Checks are programs of their own kind: the files under `tests/`,
`check_*.mlx`, test blocks, and what the check scripts
(`tools/check_*.sh`, `tests/run_*.sh`) build.

The sidebar's *Program* list shows one program: what it does not reach is
left out, and the camera flies to what is left.

Built programs are found by their symbol table
([`binaries.mlx`](../../tools/codemap/binaries.mlx)): beside the codemap
(the installed programs), in `mlx-out/bin`, and in `MLX_CODEMAP_BINARIES`
(directories, colon-separated). Each function's machine code is put on its
declaration.

### Color by

The spheres take their kind's color, or a heat scale (blue, violet,
orange, yellow) of one measure; a group is as hot as its members together:

- **Machine code:** bytes in the program shown, else in the biggest
  binary.
- **Changes:** the commits that wrote a declaration's lines as they are
  now (`git blame`,
  [`history.mlx`](../../tools/codemap/history.mlx)). The blame is read
  file by file in the background and kept in `~/.cache/mlx/codemap`, so it
  is quick the next time.
- **Complexity**, **risk** and **run time** (the profiles).

The panel says of a declaration which programs reach it, whether a check
does, and its machine code, commits, complexity, time and risk.

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

## The editor

*Open in editor* in the panel, Ctrl+O, or a click on a result while the
Editor is shown opens a declaration's file at its line
([`editor.mlx`](editor.mlx)). Up to six files are open as tabs; a dot
marks one with edits not saved, and its x closes it (Shift+click closes
one not saved). When all six hold edits, a seventh file does not open
until one is saved or closed.

What the map knows is in the editor, where the code is:

- **Colors:** keywords, strings, numbers, comments and doc comments by
  the text; every name by what the index says it is (a parameter, a
  field, a local, a type, a function, a constant, a module). The uses of
  the name at the cursor are lit (where it is written: warmer).
- **The lens row** above every function and type: how often it is used
  (a click lists the uses), the crashes that stopped in it or went
  through it, its workarounds (a click: the next one), its share of the
  time, its risk, whether it is complex, whether a program or a check
  reaches it, whether it is unused, the function with the same body or
  shape and where the same name is written again (a click opens it), its
  machine code, its commits, and whether it changed since the base.
- **On the lines:** where a crash stopped (how often, and why) or went
  through; what a workaround works around and what the language is
  missing, with *Rewrite* for dropped results and counted lengths (in
  the text: Ctrl+Z takes it back, Ctrl+S writes it). A line too long for
  them shows them in the status bar when the cursor is on it.
- **The gutter:** lines added (green), changed (blue) and removed (red)
  since the base of Changed; the compiler's errors and warnings (dots);
  crashes and workarounds. By the line numbers, a bar in the heat colors
  shows how much time each declaration took in the profiles. The pointer
  on a line number: who wrote the line, when, in which commit (`git
  blame`).
- **Hover** (as rust-analyzer shows it): where the name belongs (its
  file and the containers around it: `lib/helper.mlx › Point` for the
  field `x`, `app/main.mlx › main` for a local), its signature in the
  code's colors (a struct, enum or union with its fields or members
  under it), a value's type and where that type is declared, the doc
  comment set as text (paragraphs, `code`, items, indented code), and
  what the map measures of it.
- **Go to the declaration:** Ctrl+click or F12; back (the panel's arrow,
  Alt+Left) returns to where you came from, in the editor.
- **References:** Shift+F12 (or the lens's uses) lists every use with its
  line; Enter or a click opens one.
- **Completion:** typing a name lists what is in scope (the locals and
  parameters so far, the file's declarations, the keywords); after a dot
  the members of what is before it (`std.` the modules of std,
  `std.fs.` their functions, `point.` the fields), after `@` the
  builtins. The best fit first: what starts with the typed text, then
  what has its letters in order (`wAB` finds `writeAllBytes`). Each item
  with its signature, type or module; the chosen one's place, signature
  and doc comment at the side. A function comes with its parentheses.
  Ctrl+Space opens it with nothing typed; Up, Down, Enter or Tab, Esc.
- **Signature help:** in a call (after its `(` or a comma), the
  function's signature above the cursor with the argument lit.
- **Rename:** F2 on a name, the new name, Enter: every use the index
  knows (the declaration too), in every file; the tabs and the map read
  them again. Not while a file involved holds edits or changed since the
  map read it.
- **Folding:** the arrow left of a line that opens a function, struct,
  union or enum body (any `{` whose block spans lines) folds it into
  `{ … N lines }`; a click on that, the arrow again, or the cursor moving
  into it opens it. Ctrl+Shift+[ folds the block around the cursor,
  Ctrl+Shift+] opens it. Folds move with the edits above them.
- **Map and editor together:** the panel follows the declaration at the
  cursor; *Show in map* switches to the map and flies there.
- **The compiler's problems:** each file is checked when it opens and
  when it is saved (the compiler runs in the background, as for
  `mlx-lsp`). Errors and warnings are underlined, their message is at
  the end of the line and, for the cursor's line, in the status bar.
- **Saving** (Ctrl+S, *Save*) writes the file; the next switch to the map
  reads the code again, so the map, the findings and the Changed marks
  follow the edit.

Keys type what the keyboard layout says: AltGr types its third level
(`@`, `{`, `[`, `\` on a German keyboard), and so does Ctrl+Alt, as on
Windows, for the keys that have one. Such a key is typed, never taken as
a shortcut.

The index keeps up with the typing: a moment (300 ms) after an edit, the
tab's file is read into it again, saved or not
([`index_module.refreshFile`](../../tools/codemap/index.mlx), about 15 ms
for a file of 900 lines). Its declarations keep their symbols, so the
uses in other files still find them; the hover, the colors, the uses at
the cursor, the lens rows, completion, F12 and the workaround hints know
the text as it is. A declaration added since the map was made says it is
new; its measures (machine code, commits, time) come when the map reads
the whole code again, which it does when files change on disk.

| | |
|---|---|
| arrows, Ctrl+arrows | move, by words |
| Shift+ | select |
| Home, End, Page Up, Page Down, Ctrl+Home, Ctrl+End | line, page, file |
| click, double click, drag | the cursor, a word, a selection |
| Enter, Tab | a new line with the indent (one more after `{`), spaces to the next stop of four |
| Backspace, Delete (Ctrl: a word) | delete |
| Ctrl+Z, Ctrl+Shift+Z, Ctrl+Y | undo, redo |
| Ctrl+A, Ctrl+C, Ctrl+X, Ctrl+V | select all, copy, cut, paste |
| Ctrl+S | save |
| F12, Ctrl+click | go to the declaration |
| Shift+F12 | the references |
| F2 | rename |
| Ctrl+Shift+[, Ctrl+Shift+] | fold the block at the cursor, open it |
| Ctrl+Space | completion |
| Escape | close a list or the signature, drop the selection (the file stays) |
| Ctrl+E | back to the map |
| Ctrl+Shift+O | open the file in the outside editor (`MLX_EDITOR`, else VS Code, else `xdg-open`) |

## Build, run, profile and commit

**Build** (toolbar, Ctrl+B) saves the edited tabs and builds a program
into `ROOT/mlx-out/bin/NAME` in the background: in the editor, the
program whose `main` is the file shown, else the one chosen under
Program, else the first program that reaches the file; in the map, the
one chosen under Program. The status bar says how long it took; a failed
build opens its first error in the editor. The new binary's machine code
goes on the declarations at once.

The arrow on the button opens its list ([`run.mlx`](run.mlx)):

- **Build** (Ctrl+B) as above.
- **Run** (F5) builds, then starts the program in the root, with the
  Observatory's environment (its window opens on the same display).
  While it runs the button stops it (so does F5).
- **Profile** (Ctrl+F5) builds, then runs it under `mlx-profile` (the
  root's `mlx-out/bin`, next to the Observatory, else the `PATH`); when
  it ends its profile is read, for *Color by Run time* and the Hot
  finding.

The button does what was chosen last. Under the three, the list shows
the last lines the program wrote (its output and its errors) and how it
ended; the status bar says it too. A crash is the crash handler's line
(in red), and the crash is on the map and in the editor at once (the
crash log is read then, not two seconds later).

**Commit** is in the history (*Since HEAD* in the toolbar, Ctrl+K; the
number on it is how many files changed): at its top the changed files
(`git status`) and a message, below the commits Changed can compare
with. Enter or *Commit*
saves the edited tabs and runs `git add -A` and `git commit`; Ctrl+Enter
or *Commit and push* also runs `git push -u origin HEAD`. Git runs in the
background; the status bar says how it went (with Git's own error when it
failed), and Changed and the editor's marks follow.

**Branches** are at the history's top: the branch checked out, a click
on it lists the local branches (a click on one switches to it, `git
switch`), and *New branch* asks for a name and makes it (`git switch
-c`). The tabs without edits and the map follow the files of the branch.
Below, **the graph**: the last 30 commits of all branches (local and
remote) with their lanes, the branches and tags on each as pills, and
their subjects; a click on a commit makes it the base of Changed.

The compiler for checks and Build is `$MLX_COMPILER`, else the root's
`mlx-out/bin/compiler/mlx4`, else `mlx4` on the `PATH`.

## Using it

| | |
|---|---|
| drag | turn the view |
| right drag, Shift+drag | move it |
| wheel, +/- | come closer, go away |
| click | select |
| double click | fly there (on empty space: up a group) |
| typing, Ctrl+F, / | search; Enter or a click flies to a result |
| a query in the search | leaves out what does not answer it (see Queries); Enter flies to what is left |
| Filter (toolbar) | adds a query word to the search: first what fits the selection (in its group, calls it, called by it, uses it), then the findings and kinds |
| the bookmark, Ctrl+S (in the map) | keeps the query in the search as a view (codemap.views) |
| Since ... (toolbar) | the base of Changed: not committed yet, or one of the last 15 commits |
| a section's heading | folds it up or opens it (kept in `~/.config/mlx/observatory-sections`) |
| Escape | close a list, clear the search, then the selection, then go up a group (in the editor: only the list and the search) |
| Enter | fly to the selection |
| Alt+Up | its group |
| Backspace, Alt+Left / Alt+Right, the arrows at the top of the panel | back / forward |
| arrows | turn |
| Home | everything |
| Ctrl+O, "Open in editor" | open it at its line in the editor |
| Ctrl+Shift+O | open it in the outside editor: `MLX_EDITOR`, else VS Code (`code -g`), else `xdg-open` |
| Ctrl+E, View (sidebar) | switch between the map and the editor |
| Ctrl+B, Build | build (see Build, run, profile and commit) |
| F5, Ctrl+F5 | run, profile (the Build button's list; F5 again stops it) |
| Ctrl+K, Since ... (toolbar) | the history: commit, or commit and push |
| Ctrl+R | read the code again, keeping the selection (files changed on disk are read again by themselves) |
| Ctrl+W | close (with tabs not saved: press it twice) |

The sidebar lists the groups, and the groups inside them on the way to
what is selected, with how many files each holds. A click flies there.
The pointer on a group shows two buttons at its right: a new file and a
new folder in it. A name and Enter make it (a file without an extension
gets `.mlx`); a new file opens in the editor, and the map reads it.

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
  the places in space follow them. Once `mlx-codemap history` has counted
  the last commits, a small bar chart next to each kind shows how it went
  (the oldest commit on the left).
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

The hover, the lists (history, Filter, Build, search results, completion,
references), the signature help, the rename and new file fields and the
blame fade in over 140 ms and slide the last pixels into place, instead
of popping up.

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

## Queries and views

The search also takes a query
([`query.mlx`](../../tools/codemap/query.mlx)): words that all have to
hold, `-` before one turns it around.

| word | the declaration |
|---|---|
| `TEXT`, `name:TEXT` | its name contains TEXT |
| `kind:KIND` | is a `fn`, `struct`, `enum`, `union`, `error`, `field`, `member`, `const`, `var` or `test` |
| `in:PATH` | is in a file whose path starts with PATH |
| `file:TEXT` | is in a file whose path contains TEXT |
| `uses:NAME`, `calls:NAME` | uses (any way) or calls something named NAME |
| `by:NAME` | is called by something named NAME |
| `crashed` | a kept crash stopped in it or went through it |
| `workaround`, `workaround:KIND` | works around something (`dropped`, `counted`, `pointers`, `casts`, `unsafe`, `arena`, `floats`, `math`, `comments`) |
| `unused`, `unreachable`, `untested`, `complex`, `risky`, `hot`, `copy` | is one of that finding's |
| `changed`, `added` | changed or added since the base (`since:REV`, else HEAD) |

For example `kind:fn in:std/ uses:Allocator -untested`. What does not
answer is left out of the galaxy (a group with an answer in it stays), the
field shows how many answer, and a word it does not understand turns it
red.

Views are queries kept with the code, in `codemap.views` at the root
([`views.mlx`](../../tools/codemap/views.mlx)):

```
# a name, " = ", a query
Crashes in std = in:std/ crashed
Changed = changed
```

They are listed at the end of the sidebar (a click shows one, a second
click shows everything again, the x takes one out); `@NAME` in the search,
`--view NAME` at the start and `mlx-codemap query @NAME` show one too. The
bookmark next to the search (or Ctrl+S) keeps the query in the search as a
new view.

## The command line

The same binary with a command, or `tools/codemap/main.mlx` built on its
own:

```
mlx-codemap [-C ROOT] stats | files [TEXT] | outline FILE | find TEXT |
    show SYMBOL | def FILE:LINE:COL | refs SYMBOL | callers SYMBOL |
    callees SYMBOL | imports FILE | importers FILE | members SYMBOL |
    unused [PATH] | copies [PATH] | names [PATH] | workarounds [PATH] |
    programs | reach PROGRAM [PATH] | unreachable [PATH] |
    untested [PATH] | sizes [PROGRAM] [PATH] | complex [PATH] | cycles |
    layers | churn [PATH] | risky [PATH] | hot [PATH] | crashes [PATH] |
    rewrite PATTERN [PATH] [--apply] | history [N] | workarounds-count |
    query WORDS | query @VIEW | views | diff [BASE] [OTHER] |
    unresolved [FILE] | json
```

SYMBOL is `name`, `Container.name`, `path:name` or `FILE:LINE:COL`. The
answers are `path:line:col: kind Qualified.name` lines.

## Pictures

The pictures above are made by the app itself: `MLX_CODEMAP_DEMO` names a
script it plays (search, select, open a finding, color by a measure,
choose a program, fold a section, ...; see `runDemo` in
[`main.mlx`](main.mlx)), and it writes the frames it shows into
`MLX_CODEMAP_DEMO_OUT`. While a demo plays, flights go by its own clock:
each recorded frame moves it on by the interval asked for, so a recording
is smooth however long a frame takes to draw.

`tools/observatory_screenshots.sh [compiler] [directory]` builds what it
needs, makes the data (crashes, profiles, built programs, the history of
the last 20 commits), plays a tour and a script with a picture per
feature on lavapipe, and writes the PNGs and the GIF (with Python and
Pillow) into `screenshots/`. It takes about a quarter of an hour;
`MLX_SHOTS_ONLY=tour|features|editor` plays one part. The editor's part
edits `tools/codemap/workarounds.mlx` and puts it back (it has to be
committed).

## Checks

`tools/check_observatory.sh` does the following:

1. Validates the shader.
2. Asks the command line, also for the workarounds and a kept crash, the
   workarounds at both commits of the tree (`history`), what changed
   since the first commit (`diff`, `query changed since:HEAD~1`), queries
   and a view, and rewrites a dropped result and a counted length in a
   program that must still build and exit the same.
3. Runs the app under `tools/wayland-test-host` on a small tree, first on
   lavapipe, then on the CPU. In each run it searches, selects, checks the
   connections of what was selected, clicks a group and then its check box,
   types a query into the search, keeps it with the bookmark, adds a word
   with Filter and folds the Groups up and open again. It starts with
   `--diff HEAD~1`, so Changed holds the function changed by the second
   commit.
   `MLX_CODEMAP_TRACE`
   has the app print what it read and selected, the crashes it put on
   declarations, and how much each finding shows.
4. Compares the pictures from both runs.
5. Plays a small demo script and checks the frames it wrote. In it, the
   editor opens a file, goes to a declaration with F12, saves a name
   nobody declared and gets the compiler's error on its line, undoes it;
   the hover says where a local and a field belong; the lens's uses list
   greet's two uses and open one; Rewrite drops a dropped result's
   binding; completion offers `helper` and after the dot `greet`, and
   the signature of `greet(`; F2 renames `twice` in both its files; a
   line's blame is Git's; Build builds the program; Commit commits an
   edit with its message; Escape keeps the editor's file; the map reads
   the saved files again; completion after `std.fs.` offers the public
   functions of a std of the tree's own; a local typed and not saved is
   in the hover, with its type, a moment after.

`tools/check_crash_report.sh` checks the crash reports themselves: the
symbol table, the functions a crash names and the kept log.
