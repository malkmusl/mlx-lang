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

`storage.txt` lists the tests of `storage/domstorage` and
`storage/indexeddb` (M7), with the resources they load
(`resources/js-test*.js`, their own `resources/`) and their expected
text, taken from the same revision; tools/check_webkit_mlx_storage.sh
runs them with `webcore-dump --test` (WebKitTestRunner's testRunner,
dumpAsText) and compares the text.

`crypto.txt` lists tests of `crypto/subtle`, `crypto/` and
`crypto/workers` (M7's WebCrypto and getRandomValues, in pages and in
workers), with `crypto/resources/common.js`, `crypto/subtle/resources/`,
`crypto/workers/resources/`, `crypto/workers/subtle/resources/` and their
expected text (`platform/glib/`'s
where WebKit's GLib ports have their own), taken from the same
revision; tools/check_webkit_mlx_crypto.sh runs them as the storage
check runs its own.

`compositing.txt` lists tests of `compositing/` (M8's composited layers)
that dump their layer tree with `internals.layerTreeAsText`, with the
support files they load (`resources/ui-helper.js`,
`compositing/resources/`, `compositing/iframes/resources/`) and their
expected text (`platform/glib/`'s where WebKit's GLib ports have their
own), taken from the same revision; tools/check_webkit_mlx_compositing.sh
runs them as the storage check runs its own, with the test runner's
fonts.

`webgl.txt` lists tests of `fast/canvas/webgl` and `webgl` (M8's WebGL
on ANGLE's Vulkan backend: the GL calls, shaders, textures, readPixels,
the WebGL 2 context, wrappers of Khronos's conformance tests), with what
they load (`fast/canvas/webgl/resources/`, `webgl/resources/` and its
`webgl_test_files/`) and their expected text (`platform/glib/`'s where
the GLib ports have their own), taken from the same revision;
tools/check_webkit_mlx_webgl.sh runs them as the storage check runs its
own, and skips them where there is no Vulkan driver.
