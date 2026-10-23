# WebKit upstream: the tree the port builds

projects/webkit/upstream is WPE WebKit exactly as released: the release
tarball extracted, its top directory (`wpewebkit-2.54.1/`) removed,
committed with no other change. It is never modified (PORT.md, "The
principle: not a fork"): what the port needs of WebKit is on our side,
in the build description, the configuration, the port's own sources and
our toolchain (mlx-cc, mlxlibc, mlxlibcpp). It is the default
`WEBKIT_SOURCE` of projects/webkit/build.mlx and of the tools/ checks;
`WEBKIT_SOURCE` still names another tree.

tools/check_webkit_upstream.sh holds it to this record: the tree in the
commit checked out (`git rev-parse HEAD:projects/webkit/upstream`) must be
the one recorded below; the working tree must have no change, untracked
or ignored file in it; the last commit that touched it must be the
import or have changed this file too (an upgrade is one commit); and,
when the tarball is at hand (`WEBKIT_TARBALL`, by default the name below
in /home/user/csrc/wpe), the tarball must be the one recorded (size,
SHA-256) and the tree must match it file by file (contents, executable
bit, symbolic links; the tarball holds
Source/WebKit/Resources/wpe/wpe-theme.css twice, alike).

## The import

The lines below are read by tools/check_webkit_upstream.sh.

    version: WPE WebKit 2.54.1
    tarball: wpewebkit-2.54.1.tar.xz
    tarball-size: 46206356
    tarball-sha256: 0de8bdfba011f9cd63ba9a9171fa6038dd2c08237894f4082abbb821f29d3fe9
    tarball-top: wpewebkit-2.54.1
    imported: 2026-10-23, commit a782913
    tree: 94a8b84f30b878f4f72d473c3fd9ec406d7d748c

Files: 38,844 (188 executable, one symbolic link), 525 MB.

The tree used before it, /home/user/csrc/wpe/wpewebkit-2.54.1, had been
patched by hand in two headers (Source/WTF/wtf/SIMDHelpers.h,
Source/bmalloc/libpas/src/libpas/pas_segregated_page_inlines.h) for what
mlx-cc could not yet compile; mlx-cc compiles both as released now
(PORT.md, "The toolchain, not WebKit").

## Upgrading WebKit

In one commit, and only so:

1. Get the new release's tarball (wpewebkit-X.Y.Z.tar.xz from
   wpewebkit.org) and check its signature or published checksum.
2. Replace the tree: `git rm -r -q projects/webkit/upstream`, then
   `mkdir projects/webkit/upstream && tar -xJf wpewebkit-X.Y.Z.tar.xz
   -C projects/webkit/upstream --strip-components=1`, then
   `git add -f projects/webkit/upstream` (`-f`: the repository's ignore
   rules must not leave out any of WebKit's files).
3. Update the block above: version, tarball, its size (`stat -c %s`) and
   SHA-256 (`sha256sum`), its top directory, the date, and the new tree
   (`git write-tree --prefix=projects/webkit/upstream/` after the add).
4. Commit the tree and this file together, then run
   `WEBKIT_TARBALL=path/to/wpewebkit-X.Y.Z.tar.xz
   tools/check_webkit_upstream.sh` (the commit is named by its hash
   in `imported:` only for the record).

Then the port follows: the source lists (projects/webkit/lists), the
configuration (config.mlx, from the new release's Options*.cmake), the
generators, a full rebuild (a new tree builds everything again) and the
checks of PORT.md. What the new release needs of the toolchain is fixed
there, never in projects/webkit/upstream.
