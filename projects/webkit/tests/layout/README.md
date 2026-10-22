# WebKit layout tests for the Mlx port (M3)

Tests of WebKit's `LayoutTests`, taken unchanged from
https://raw.githubusercontent.com/WebKit/WebKit/537776966344c4982751692dd475c94124504271/LayoutTests/
— commit 537776966344c4982751692dd475c94124504271, the tag `wpewebkit-2.54.1`
of WebKit/WebKit, the revision of the WPE WebKit 2.54.1 tarball the port
builds. Each test keeps its own header and licence (the CSS 2.1 tests are
the W3C's, the others WebKit's, BSD or LGPL); `resources/Ahem.ttf` is the
Ahem font of the tests.

`LayoutTests/` mirrors the repository's paths: a test, the support files
it loads (`css2.1/support/`), and its render tree as WebKitTestRunner
dumps it, the first of `platform/wpe/`, `platform/glib/` and the generic
`-expected.txt` that exists. `tests.txt` lists the tests, grouped by what
they lay out: those of the `ahem` groups set their text in Ahem (every
glyph a square of the font size, so the tree is the same whatever the
system's fonts); the `liberation` ones use WebKitTestRunner's GLib fonts
(Liberation Serif, Sans and Mono, as its `fonts.conf` names them).

tools/check_webkit_mlx_layout.sh runs `webcore-dump` on each test with
those fonts and compares its render tree with the expected one.
