# Splitting projects/ into their own repositories

The projects under `projects/` are to move into repositories of their own
on GitHub, each brought back into mlx-lang as a git submodule at the path
it has now. Their histories are prepared as local branches of this
repository (made with `git subtree split`, nothing pushed); mlx-lang's own
tree is unchanged until the repositories exist.

## Repositories and branches

| Repository | Local branch | Made of |
| --- | --- | --- |
| `malkmusl/voyager` | `split/voyager` | `projects/voyager` at `voyager/` and `projects/webkit` at `webkit/` (two subtree splits joined with `git subtree add --prefix=voyager` / `--prefix=webkit`), `lattice/README.md` (pointing to `webkit/LATTICE.md`) and `VERSION` (`0.1`) at the root |
| `malkmusl/mlx-desktop` | `split/mlx-desktop` | `projects/desktop` |
| `malkmusl/mlx-observatory` | `split/mlx-observatory` | `projects/observatory` |
| `malkmusl/mlx-coreutils` | `split/mlx-coreutils` | `projects/coreutils` |
| `malkmusl/mlx-shell` | `split/mlx-shell` | `projects/shell` |
| `malkmusl/mlx-console` | `split/mlx-console` | `projects/console` |
| `malkmusl/mlx-init` | `split/mlx-init` | `projects/init` |
| `malkmusl/mlx-network` | `split/mlx-network` | `projects/network` |
| `malkmusl/mlx-android` | `split/mlx-android` | `projects/android` |
| `malkmusl/mlx-filemanager-shared` | `split/mlx-filemanager-shared` | `projects/filemanager-shared` |

Each `split/mlx-*` branch is the subtree split of its directory (its
files at the root, every commit that touched it) and one commit adding
`VERSION` (`0.1`) at the root. The `split/raw-*` branches are the plain
subtree splits the others were made from; they can be deleted once the
repositories are pushed.

`split/voyager` holds WebKit's upstream tree (`webkit/upstream`, about
525 MB); its objects are the ones this repository already has, so the
split itself costs no space, but its first push uploads all of them.

The branches were made from the commit before the one adding this file
(the one adding `projects/voyager/VERSION`). Work committed under
`projects/` after that is brought over by splitting again: `git subtree
split --prefix=projects/NAME -b split/raw-NAME` continues the same history
(the same commits come out), and the new commits are merged into the
split branch.

## Finishing once the repositories exist

1. Push each split branch to its repository's `main`:

       git push git@github.com:malkmusl/voyager.git split/voyager:main
       git push git@github.com:malkmusl/mlx-desktop.git split/mlx-desktop:main
       ... (one per row above)

2. In mlx-lang, replace each `projects/NAME` by a submodule at the same
   path:

       git rm -r projects/NAME
       git submodule add https://github.com/malkmusl/mlx-NAME.git projects/NAME

   (`projects/desktop` is `mlx-desktop`, and so on.) For these the paths
   inside mlx-lang stay the same; nothing else changes.

3. Voyager and WebKit become one submodule at `projects/voyager`:

       git rm -r projects/voyager projects/webkit
       git submodule add https://github.com/malkmusl/voyager.git projects/voyager

   Then every path of the two moves, and every mention of them changes:

   - `projects/webkit/...` becomes `projects/voyager/webkit/...`
   - `projects/voyager/...` becomes `projects/voyager/voyager/...`

   in:

   - `build.mlx` (`@import("./projects/webkit/build.mlx")`)
   - `projects/webkit/build.mlx` itself (its `projects/webkit/` and
     `projects/voyager/` paths: sources, `org.mlx.voyager.png`,
     `projects/voyager/VERSION`), `projects/webkit/PORT.md`,
     `projects/webkit/README.md` and the other files inside the two
     directories that name these paths (`git grep -l projects/webkit --
     projects/webkit projects/voyager ':!projects/webkit/upstream'`)
   - `CLAUDE.md` ("WebKit (projects/webkit) is never modified", "see
     projects/webkit/PORT.md")
   - tools: `check_wpe_jsc.sh`, `check_webkit_upstream.sh`,
     `check_webkit_mlx_port.sh`, `check_webkit_mlx_paint.sh`,
     `check_webkit_mlx_network.sh`, `check_webkit_mlx_layout.sh`,
     `check_wpe_wtf.sh`, `check_webkit_mlx_storage.sh`,
     `check_webkit_generators.sh`, `check_voyager.sh`,
     `check_jsc_mlxlibc.sh`, `check_zlib.sh`, `check_glib.sh`,
     `check_codegen_epoch.sh`, `check_xml2.sh`, `check_webp.sh`,
     `check_webkit_mlx_webgl.sh`, `check_webkit_mlx_crypto.sh`,
     `check_webkit_mlx_compositing.sh`, `check_webcore_generators.sh`,
     `check_png.sh`, `check_lcms2.sh`, `check_jpeg.sh`,
     `check_harfbuzz.sh`, `check_freetype.sh`,
     `install_compositor_session.sh`, `voyager_screenshots.sh`
   - docs: `docs/internals/build-time-notes.md`,
     `docs/reference/build-system.md`,
     `docs/internals/allocators-and-containers.md`, `docs/ROADMAP.md`
   - elsewhere: `README.md`, `projects/observatory/README.md`,
     `compiler/libc/xml2/README.md`, `compiler/libc/icu/README.md`,
     `compiler/libc/spirv/api/spirv_tools.mlx`,
     `std/src/build/jobs.mlx`, `std/src/git.mlx` (comments)

   `git grep -n "projects/webkit\|projects/voyager"` lists them all;
   after the change it must find only the new paths.

4. The cloud environment (and every checkout) must initialize the
   submodules: `git clone --recurse-submodules`, or `git submodule update
   --init` in the environment's setup script; the build fails without
   them.

## Build numbers

Every repository has its own `VERSION` at its root (`MAJOR.MINOR`, `0.1`
now) and its own count, by the rule of docs/reference/build-system.md
("Versions and build numbers"): `BUILD` is the commits reachable from
HEAD since `VERSION` last changed, the changing commit counted; every
commit of HEAD until it first changes. std.git counts a submodule on its
own (its `.git` file leads into mlx-lang's `.git/modules/NAME`), so each
project keeps its numbers inside mlx-lang:

- mlx-lang: `VERSION` at the root, the toolchain's version (mlx4, mlx-cc,
  mlx-ld `--version`).
- Voyager: today `projects/voyager/VERSION` in mlx-lang, which counts all
  of mlx-lang's commits (Voyager 0.1.BUILD). In `malkmusl/voyager` it is
  the root `VERSION`, and `voyagerBuildHeader` (the WebKit build.mlx)
  reads `git.buildNumberOf("projects/voyager", "VERSION", ...)`: the build
  then counts the voyager repository's commits (the split history, which
  holds the commits that touched projects/voyager or projects/webkit).
  `voyager/VERSION` is then left over and can be removed. The user agent
  keeps `Voyager/MAJOR.MINOR`, without the build.
- Each `mlx-*` repository: its root `VERSION`, read by whatever shows its
  version as `git.buildNumberOf("projects/NAME", "VERSION", ...)`.
