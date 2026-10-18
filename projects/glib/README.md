# mlx-glib

GLib, GObject and GIO for the C and C++ programs built here, in Mlx, with
GLib's C API (2.80). WebKit's base library (WTF) for the WPE port
includes `<glib.h>`, `<glib-object.h>`, `<gio/gio.h>` and `<glib-unix.h>`
as it would GLib's headers, and links `libmlxglib.so` in place of
`libglib-2.0`, `libgobject-2.0` and `libgio-2.0`. No GLib code is used:
the library is this repository's own, written from GLib's documented
behaviour and checked against what GLib prints for the same programs.

The interface is written once, in Mlx: the modules of `api/` declare the
types, constants, functions and macros C sees (`G_OBJECT`, `g_new`,
`g_signal_connect`, `G_VALUE_INIT`... are Mlx functions and constants
there). `mlx-cheaders` (projects/cc) makes the C headers of them into
`mlx-out/include/mlx-glib/`; there are no headers written by hand.

    ./mlx build glib           the library and its headers
    tools/check_glib.sh        builds it, runs the tests, compiles WTF's GLib sources

The scope is what WTF's WPE sources use (`wtf/glib/*.cpp`,
`generic/MainThreadGeneric.cpp`, `generic/WorkQueueGeneric.cpp`,
`unix/`, `linux/`, `posix/`) and JavaScriptCore's for WPE (its GLib API,
`API/glib`, and the remote inspector's `inspector/remote/glib`), with the
behaviour GLib gives it: a real main loop, threads, memory, strings,
containers, errors, GVariant, GUri, command line options, the GObject
type system with properties and signals, and the part of GIO those
sources reach.

## Parts

| File | What |
|---|---|
| `api/` | The interface in Mlx, a module per header (GLib's names, 2.80); `glib.mlx`, `glibobject.mlx`, `gio.mlx` and `glibunix.mlx` are `glib.h`, `glib-object.h`, `gio/gio.h` and `glib-unix.h` |
| `glib.mlx` | The library's root (`--plugin --stack-arena`), importing the parts in order |
| `memory.mlx`, `sys.mlx`, `varargs.mlx` | What the others stand on: memory access, C strings and growing text, the system calls, reading C's variable arguments (`...` and `va_list`) |
| `alloc.mlx` | `gmem.h` |
| `threads.mlx` | `gthread.h`, `gatomic.h`: threads (POSIX), mutexes, recursive mutexes, read-write locks, conditions, private keys, `g_once` |
| `errors.mlx`, `messages.mlx` | `gquark.h`, `gerror.h`; `gmessages.h` (log levels, handlers, the default handler's output, `G_MESSAGES_DEBUG`, fatal masks, criticals of failed checks) |
| `utils.mlx` | `gutils.h`: the environment, user and system directories (XDG), the program name |
| `strings.mlx` | `gstrfuncs.h`, `gstring.h`, `gunicode.h`, `gconvert.h`, `gbase64.h`: strings, ASCII classes and case, UTF-8 (validation, iteration, lengths and offsets, making valid), conversions between UTF-8, UTF-16 and UCS-4, escaping, `g_strdup_printf` with C's formats |
| `lists.mlx`, `hash.mlx`, `arrays.mlx` | `glist.h`, `gslist.h`; `ghash.h` (GHashTable); `garray.h`, `gbytes.h` (GArray, GPtrArray, GByteArray, GBytes) |
| `files.mlx` | `gfileutils.h`, `gstdio.h`, `gdir.h`, `gmappedfile.h`, file names and URIs, host names |
| `checksum.mlx`, `misc.mlx` | `gchecksum.h` (MD5, SHA-1, SHA-256, SHA-384, SHA-512); `gpattern.h`, `gtimer.h`, `gkeyfile.h`, `gshell.h`; `ggettext.h` and `gi18n-lib.h` (no message catalogs: a message is the program's own) |
| `options.mlx` | `goption.h`: contexts and groups of entries; parsing long, grouped short and group-prefixed options, `--`, the remaining arguments and callbacks, every value put back on an error, with GLib's messages |
| `main.mlx` | `gmain.h`: contexts, loops, sources (idle, timeout, child watch, Unix descriptors, ready times), thread-default contexts, `poll` with the context's wake-up descriptor |
| `variant.mlx` | `gvariant.h`, `gvarianttype.h`: every type, the serialized form GLib reads and writes (its framing offsets, alignment, the normal form of data that does not hold together), builders, iterators, dictionaries, `g_variant_print` |
| `uri.mlx` | `guri.h`: parsing with GLib's flags, building, escaping and unescaping, parameters |
| `datetime.mlx` | `gdatetime.h`, `gtimezone.h`: UTC, the local zone (the C library's), fixed offsets |
| `markup.mlx`, `spawn.mlx` | `gmarkup.h` (the parser and escaping); `gspawn.h`, `glib-unix.h` (spawning with pipes, `g_spawn_command_line_sync`, Unix signals as sources, pipes, descriptors) |
| `types.mlx` | `gtype.h`: fundamental and derived types, classes, interfaces with prerequisites, instance private data, type data by quark |
| `values.mlx` | `gvalue.h`, `gboxed.h`, `genums.h`, `gvaluearray.h`: values of every fundamental, transformations, boxed types, enums and flags |
| `params.mlx` | `gparam.h`: the parameter specs of every fundamental, defaults, validation |
| `closures.mlx` | `gclosure.h`: C closures, the generic marshaller, class closures |
| `signals.mlx` | `gsignal.h`: signals with details, accumulators, emission hooks, the run stages, blocking, connecting (also after, swapped, with closures), lookups and queries |
| `objects.mlx` | `gobject.h`: GObject with references (toggle and weak ones too), construction properties, `notify` and its freezing, data, `GInitiallyUnowned` |
| `ioerrors.mlx` | `gioerror.h` and what GIO's types share (errno to `GIOErrorEnum`, defining types) |
| `cancellable.mlx` | `gcancellable.h`: with its signal, descriptor and source |
| `task.mlx` | `gasyncresult.h`, `gtask.h`: tasks returning at once, from idle sources, from threads (and synchronously) |
| `localfile.mlx` | `gfile.h`: local files (paths, URIs, reading, replacing with etags, deleting, making directories); `gfilemonitor.h` on inotify, with GIO's rules for pending changes and renames |
| `socket.mlx` | `gsocket.h`, `giostream.h`, `gsocketconnection.h`: sockets (always non-blocking underneath, as in GIO), their sources, and Unix and TCP connections |
| `network.mlx` | `ginetaddress.h`, `gsocketaddress.h`, `ginetsocketaddress.h`, `gsocketconnectable.h`, `gsocketlistener.h`, `gsocketservice.h`, `gsocketclient.h`: IPv4 and IPv6 addresses, listening (a service accepts in its main context and emits "incoming"), connecting to "host:port" at once or asynchronously |
| `resource.mlx`, `inflate.mlx` | `gresource.h`: the GVDB files `glib-compile-resources` writes, compressed entries (an inflater of its own), the registry, static resources |
| `application.mlx` | `gapplication.h`: application ids and the default application |
| `dbus.mlx` | `gdbus*.h`: bus addresses, proxies, errors, introspection data; see below |
| `tests/` | C programs built by mlx-cc against the library, each with the output GLib 2.80 gives (`NAME.expected`, the exit status last); `tests/wtf/` the WPE configuration and the WTF sources compiled by the check |

## Tests

`tools/check_glib.sh`:

- builds the library and its headers; checks that every function the
  headers declare is in the library (1508);
- compiles `glib.h`, `glib-object.h`, `glib-unix.h`, `glib/gstdio.h`,
  `gio/gio.h` and `gobject/gvaluecollector.h` together as C and as C++;
- runs `tests/strings.c`, `mainloop.c`, `variant.c`, `uri.c`,
  `gobject.c`, `system.c`, `gio.c`, `collect.c` (G_DECLARE_FINAL_TYPE,
  G_VALUE_COLLECT_INIT and G_VALUE_LCOPY, g_alloca), `options.c`
  (GOption, gettext) and `network.c` (addresses, a service, a client,
  their errors) and compares what they print with what the same programs
  print on GLib 2.80;
- compiles WTF's GLib-based sources for WPE (`tests/wtf/files.txt`: the
  14 of `wtf/glib`, `MainThreadGeneric.cpp`, `WorkQueueGeneric.cpp` and
  those of `unix/`, `linux/`, `posix/`) with mlx-cc on these headers and
  WPE's configuration (`tests/wtf/wpe-config.txt`, the `cmakeconfig.h`
  of a default WPE build as NAME VALUE lines), where the WebKit source is
  (`WEBKIT_SOURCE`).

## What is not here, and how it behaves instead

Where GLib itself has a case for something that is not available, the
library takes that case, so that callers see what they would on a
system without it:

- **D-Bus**: the library does not speak the D-Bus protocol. Bus
  addresses are found as GIO finds them; connecting fails as GIO's does
  when no bus answers ("Could not connect: ..." with the code of the
  system's error for a socket that is not there, `G_IO_ERROR_FAILED`
  "Cannot autolaunch D-Bus without X11 $DISPLAY" for a session bus
  without an address), and with `G_IO_ERROR_NOT_SUPPORTED` when a bus
  does answer. Proxies are therefore never made; the asynchronous calls
  report the same error to their callbacks.
- **GApplication** is the object, its id and flags and the default
  application (none until one is made, so `g_application_get_default`
  is `NULL` as in a program without one); running it (registration on
  the bus, the main loop of `g_application_run`) is not here.
- **Time zones**: UTC, the local zone (the C library's `localtime_r`,
  which follows `TZ` and the system's zone files) and fixed offsets;
  other named zones are not available: `g_time_zone_new_identifier`
  returns `NULL` for them, as GLib does for an identifier it cannot
  load.
- **Unicode** beyond UTF-8's structure (character properties, case
  mapping and normalization of `gunicode.h`) is not here; WebKit takes
  those from mlx-unicode.
- **Files** are local paths; another URI scheme gives a file that knows
  only its URI (GIO's GDummyFile without a VFS module), and operations on
  it fail with `G_IO_ERROR_NOT_SUPPORTED`. GFileInfo, enumerators and
  streams (GInputStream, GOutputStream, `g_resource_open_stream`) are not
  here.
- **GVariant**'s text form is printed but not parsed
  (`g_variant_parse`, `g_variant_new_parsed` are not here).
- **GObject**: class private data (`g_type_add_class_private`) and
  dynamic types (`GTypePlugin`, `GTypeModule`;
  `g_type_register_dynamic` reports a critical and returns 0) are not
  here. `G_DECLARE_FINAL_TYPE` and `G_DECLARE_DERIVABLE_TYPE` are (as C
  the header carries, `c_macros`), the `G_DEFINE_*` macros not yet;
  `g_autoptr` is not (mlx-cc has no cleanup attribute): the cleanup
  functions are declared for code that calls them.
- **GOption**'s help (`--help`, `g_option_context_get_help`) is not here:
  `--help` is an unknown option unless an entry has that name.
- **Networking**: names are resolved by the C library at once, before
  connecting (GResolver, proxies, GNetworkAddress and TLS are not here);
  Unix socket addresses are not here.
- Everything of GLib, GObject and GIO that WTF and JavaScriptCore do not
  use (GIO's settings, TLS, GSubprocess, GVfs...).

The library's messages, errors and their texts are GLib's, in English;
there is no translation.

## mlx-cheaders

GLib's headers needed more of C than mlx-unicode's did; mlx-cheaders now
also writes: function pointer types (`const c_function_pointers =
true`), variadic functions (`c_varargs`) and `va_list` (`c_va_list`), the
tag spelling `typedef struct _T T`, unions (named and in place), macros
of any case (`pub inline fn`), `p.*.field` as `p->field`, `@offsetOf`,
`@sizeOf(@TypeOf(e))` as `sizeof(e)`, constants made by casts, C's
`signed char`, `unsigned char` and `float`, the system headers a
header brings (`const c_system_headers = "a.h b.h"`), and C as it is at
the end of a header (`const c_macros = "..."`) for the macros Mlx cannot
say: those that paste or quote tokens or read `va_arg`
(`G_DECLARE_FINAL_TYPE`, `G_VALUE_COLLECT_INIT`, `G_BEGIN_DECLS`). The
unicode headers come out as before.
