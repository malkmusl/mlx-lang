# MLXIPC

MLXIPC is the desktop's message bus: `mlx-ipcd`, a session bus that D-Bus
programs use as they would dbus-daemon, with what the D-Bus session bus
lacks: permissions per app, permissions services ask about, and direct
channels between two apps past the bus, and a wire format of its own for
Mlx programs next to D-Bus on the same socket (below). It is written in
Mlx on `std.dbus` and `std.ipc` (no libc, no libdbus). MLX Audio
(`projects/desktop/audio`, the PipeWire replacement) runs on it.

```sh
mlx4 projects/desktop/ipc/main.mlx -o mlx-ipcd
mlx4 projects/desktop/ipc/tool.mlx -o mlx-ipc
./mlx-ipcd --run mlx-compositor          # a session on this bus
tools/check_ipc.sh                       # the end-to-end check
```

The desktop session (`mlx-session`, installed by
`tools/install_compositor_session.sh`) runs the compositor on mlx-ipcd,
so everything started in it gets `DBUS_SESSION_BUS_ADDRESS`
(`MLX_SESSION_IPC=no` turns that off).

## Compatible with D-Bus

The wire protocol, authentication (EXTERNAL, Unix descriptor passing),
unique and well-known names with queues and replacement, match rules
(type, sender, interface, member, path, path_namespace, destination,
arg0, arg0namespace), signals, `org.freedesktop.DBus` (Hello,
RequestName, ReleaseName, ListNames, ListActivatableNames, NameHasOwner,
GetNameOwner, ListQueuedOwners, AddMatch, RemoveMatch,
GetConnectionUnixUser, GetConnectionUnixProcessID,
GetConnectionCredentials, GetId, StartServiceByName,
UpdateActivationEnvironment, ReloadConfig), the Monitoring interface
(BecomeMonitor: dbus-monitor, busctl monitor), Peer and Introspectable.
A connection that leaves while it owes replies has them answered with
NoReply.

Activation: a call to a name nobody owns starts the service whose
`.service` file provides it (`--services DIR`,
`$XDG_RUNTIME_DIR/dbus-1/services`, `$XDG_DATA_HOME/dbus-1/services`,
each of `$XDG_DATA_DIRS`), with the bus's address in its environment;
the call waits until the service owns the name (or it ended:
Spawn.ChildExited, or 25 seconds went: Spawn.Timeout). `--start NAME`
starts a service with the bus (the session starts MLX Audio so, for its
PulseAudio apps).

dbus-send, gdbus, busctl and dbus-monitor are checked against it
(`tools/check_ipc.sh`).

`mlx-ipcd --system` is the same bus as the system's: it listens at
`/run/mlx/bus` (or `--address`) with the socket open to every user, root
alone may own names, the policy comes from `/etc/mlx/ipc.conf`, and it
starts services only from `--services DIR` (the session's are not
root's to start). Our programs look for it there by themselves:
mlx-netd (`projects/network`) answers on it, and mlx-net, the top bar
and mlx-settings ask it there; other programs reach it through
`DBUS_SYSTEM_BUS_ADDRESS=unix:path=/run/mlx/bus` (the compositor's logind
client reads that variable). `tools/install_network.sh` installs it as
`mlx-ipcd.service` (systemd) and for mlx-init. Under systemd it is a
`Type=notify` service: it sends `READY=1` to `NOTIFY_SOCKET` once the
socket takes connections, so mlx-netd and mlx-login, ordered after it,
find the bus there (the variable is not passed on to the services it
starts). It is where a display
manager of the desktop's own would offer its login and seat services.

Services that do something for an app ask the bus whether that app may
(`org.mlx.IPC.Check`, below): MLX Audio for `audio.*`, mlx-netd for
`network.control` (switching links, joining and forgetting networks).

## Permissions per app

dbus-daemon's policy is per user, and every program of a desktop session
runs as the same user. MLXIPC's is per app, as in hyprtavern:

- Each connection is an app: its Flatpak id (from
  `/proc/PID/root/.flatpak-info`; it is then **sandboxed**) or its
  program's file name (`/proc/PID/exe`). `mlx-ipc apps` lists them.
- What an app may do with a name: **own** it, **talk** to it (call its
  methods, send it signals) and **see** it (hear its signals, find it in
  ListNames, GetNameOwner, NameOwnerChanged). A rule names the connection
  it talks to by any name that connection owns, as dbus-daemon's
  `send_destination` does.
- And **use** a service's permission: `audio.play`, `audio.record`,
  `audio.monitor`, `audio.control` (MLX Audio), `network.control`
  (mlx-netd, on the system bus). Services ask the bus
  (`org.mlx.IPC.Check(s app's connection, s permission) -> b`) before
  they do what it names, so the rules for every service are in one
  place.
- By default an unsandboxed app may do anything but the two permissions
  below (as on any session bus), and a sandboxed one may own its own
  names (its id and the names under it), talk to and see those, other
  connections of the same app, the portals (`org.freedesktop.portal.*`),
  `org.freedesktop.Notifications` and MLX Audio (`org.mlx.Audio`), and
  use `audio.play`; for the top bar it may also own an MPRIS player's
  name (`org.mpris.MediaPlayer2.*`) and a tray item's
  (`org.kde.StatusNotifierItem-*`) and talk to the tray's watcher
  (`org.kde.StatusNotifierWatcher`). It sees nothing else, not even the
  other connections' unique names. Replies to calls an app made always come back; a reply
  nobody asked for needs the rules.
- The microphone (`audio.record`) and the sound of other apps
  (`audio.monitor`) are **asked** for every app, sandboxed or not: the
  user decides (below). A rule (`allow firefox use audio.record`, or the
  answer remembered) settles it.
- `~/.config/mlx/ipc.conf` (`--policy FILE`) adds rules, read again when
  it changes (and on SIGHUP, ReloadConfig, `mlx-ipc reload`). The last
  rule that applies wins:

```
# MLXIPC permissions
group media obs com.obsproject.Studio
allow @media use audio.record
ask org.example.Chat use audio.record
allow org.mozilla.firefox talk org.freedesktop.secrets
deny firefox talk org.freedesktop.secrets
deny @sandboxed own *
strict            # unsandboxed apps get nothing by default either
```

Subjects: an app, `@GROUP`, `@sandboxed`, `@unsandboxed` or `@all`.
Names: a name, `prefix.*` (prefix and the names under it), `*` or `self`.
A refused call gets `org.freedesktop.DBus.Error.AccessDenied`. `ask`
(for `use` only) has the user decide.

## Asking the user

When the decision is to ask, the Check waits and the bus asks the
permission agent: `org.mlx.PermissionAgent.Ask(s app, s permission, b
sandboxed) -> (b allowed, b remember)` on whoever owns that name. It is
`mlx-permissions` (`projects/desktop/permissions`), started from its
service file when nobody owns the name yet; it shows a dialog. The answer
goes to every Check waiting on the same question. Remembered, it becomes
a rule of `ipc.conf` (the app's other lines for that permission go);
otherwise it holds until the bus ends. With no agent to be had, or no
answer within two minutes, the answer is no. The agent takes Ask from the
bus only.

Whenever the rules change (the file, a remembered answer, SetPermission),
the bus sends `org.mlx.IPC.PermissionsChanged` and services check again:
MLX Audio ends a recording whose permission went.

The bus keeps which app asked for which permission
(`$XDG_STATE_HOME/mlx/ipc-seen`, `~/.local/state/mlx/ipc-seen`), for
mlx-settings' Apps category (`projects/desktop/settings/permissions.mlx`),
which shows every app with Allow, Ask, Deny or Default per permission:

- `org.mlx.IPC.ListPermissions() -> a(sssb)`: app, permission, decision,
  sandboxed. The decision is `allow`, `deny` or `ask` (a rule of
  `ipc.conf`), `session-allow` or `session-deny` (an answer for this
  session), or `default-allow`, `default-deny`, `default-ask` (what the
  defaults say).
- `org.mlx.IPC.SetPermission(s app, s permission, s decision)`: `allow`,
  `deny`, `ask`, or `default` (the app's rule for it goes). It needs the
  permission `ipc.manage` (unsandboxed apps have it, sandboxed ones not).

## Direct channels

`org.mlx.IPC.Connect(s NAME) -> h` takes two apps past the bus, the way
hyprtavern's bus is only for finding each other: the bus makes a socket
pair, hands one end to the caller and the other to NAME's owner (a call
`org.mlx.IPC.Peer.Connected(h channel, s caller, s app)` that wants no
reply), and from then on the two talk directly, in D-Bus messages or a
protocol of their own; the bus never sees that traffic. The policy
decides as for a call: the caller must be allowed to talk to NAME. The
bus remembers who opened each channel, so the service can still Check
the app's permissions after the app left the bus. MLX Audio's streams go
this way.

`org.mlx.IPC` also answers Check(s, s) -> b, CheckProcess(u pid, s
permission) -> b (as Check, for an app that reached the service past the
bus: MLX Audio's PulseAudio socket names its peer process), GetAppId(s)
-> (s app, b sandboxed), ListApps() -> a(ssbu), ListPermissions() ->
a(sssb), SetPermission(sss) and Reload(). Sandboxed apps may not Check.

## The native wire format

D-Bus is what the bus speaks to everyone else's programs; to ours it
speaks MLXIPC's own format, `std.ipc` (`std/src/ipc.mlx`), on the same
socket. The bus tells the two apart by the first byte a connection sends
(a native one starts with the preamble `MLXI`, a D-Bus one with NUL) and
carries every message between them: an Mlx program speaking natively
calls a D-Bus service and is called by dbus-send and gdbus, a native
service's signals reach D-Bus listeners, and dbus-monitor sees it all.
`mlx-ipc --native` is the same tool on the native format
(`native.mlx`); `tools/check_ipc.sh` runs it next to dbus-send, gdbus and
dbus-monitor, and `tests/343_ipc_wire_runtime.mlx` checks the format
itself.

Both formats carry the same messages (calls, replies, errors and signals,
with the same header fields and the same type signatures), so nothing is
lost in the carrying; what the native format leaves out is what D-Bus
costs:

- no SASL exchange: the socket's credentials (SO_PEERCRED) are the
  connection's authentication, as EXTERNAL's are anyway;
- one round trip before the first call instead of three: the preamble and
  Hello go out together, the bus's preamble and the unique name come back
  together;
- little-endian only, no padding between values, booleans of one byte,
  arrays counted in elements, a message framed by its size first.

The preamble is 8 bytes each way: `MLXI`, the version (1), the flags
(1: Unix descriptors), two zero bytes. A message is a header of 20 bytes,
its fields and its body:

```
u32 size          the whole message (at most 65536)
u8  kind          1 call, 2 reply, 3 error, 4 signal
u8  flags         1 no reply expected, 2 no auto start
u16 descriptors   Unix descriptors sent with it
u32 serial        never 0
u32 replySerial   the call answered (replies and errors; 0 otherwise)
u16 fieldsLength  bytes of fields that follow
u16 0
fields            each: u8 code, u16 length, the bytes (no NUL)
body              its values, as the SIGNATURE field says
```

The field codes are D-Bus's (1 path, 2 interface, 3 member, 4 error name,
6 destination, 7 sender, 8 signature); a field a reader does not know is
skipped. Values follow their D-Bus signature code, one after the other:
`y` and `b` one byte, `n q` two, `i u h` four (`h` is the descriptor's
index among the message's), `x t d` eight; `s` and `o` a u32 length, the
bytes and a NUL; `g` a u8 length, the bytes and a NUL; `a` a u32 count
and that many elements; a struct its fields; a dictionary entry its key
and value; `v` a signature of one complete type and the value.

The bus keeps D-Bus form inside (the routing, the policy, the monitors):
a native message is carried to it as it arrives and back to the native
format for a native receiver, so between two native programs it is
carried twice. A D-Bus body larger than 65536 bytes once padded cannot be
carried, and a native message that cannot be carried drops its sender.
`org.mlx.IPC.GetProtocol(s connection) -> s` says what a connection
speaks, `mlx` or `dbus`, for a program that would rather talk natively
past the bus: over a direct channel two native programs speak the format
with no bus between (`std.ipc`'s `attach` on the channel's descriptor).

## mlx-ipc

```
mlx-ipc apps               the connections: unique name, app, sandboxed, process
mlx-ipc names              the names on the bus
mlx-ipc reload             the bus reads its policy again
mlx-ipc serve NAME         owns NAME, answers org.mlx.Echo.Echo(s)
mlx-ipc call NAME TEXT     calls NAME's org.mlx.Echo.Echo(TEXT)
mlx-ipc connect NAME       a direct channel to NAME; prints what comes through
mlx-ipc permissions        what apps asked for: app, permission, decision
mlx-ipc permit APP PERMISSION allow|deny|ask|default
                           a rule for it in ipc.conf (default: none)
mlx-ipc protocol NAME      what NAME's owner speaks: mlx or dbus
mlx-ipc --native COMMAND   on the native wire format: names, apps,
                           protocol NAME, serve NAME, call NAME TEXT,
                           connect NAME (prints the first native message's
                           string), emit TEXT (the signal org.mlx.Echo.Said)
```

## Modules

| File | What it does |
| --- | --- |
| `main.mlx` | mlx-ipcd: options, listening, signals (signalfd), the policy's directory (inotify), the event loop (epoll, std.turns), `--run` |
| `state.mlx` | the records: bus, clients, names, match rules, waiting replies, policy, activation |
| `bus.mlx` | clients, authentication (D-Bus's SASL, or the native preamble), names, match rules, routing, monitors; native messages carried to D-Bus form and back |
| `driver.mlx` | what the bus answers itself (org.freedesktop.DBus, org.mlx.IPC) and its signals |
| `policy.mlx` | who a connection is, the rules file, the decisions, rules written back (`setRule`) |
| `prompts.mlx` | asking the user through the permission agent, answers for the session, the apps that asked, ListPermissions and SetPermission |
| `activation.mlx` | `.service` files, starting services, calls waiting for them |
| `tool.mlx` | mlx-ipc |
| `native.mlx` | `mlx-ipc --native`: the commands on the native wire format (`std.ipc`) |

## Next

- The native wire format between two native connections passed through
  as it is, without the D-Bus form in between; interfaces declared in Mlx
  (typed messages as hyprwire's) with their signatures and the reading and
  writing of their arguments generated from the declaration.
- polkit: the same agent asks for the system's actions (README of
  `projects/desktop/permissions`).
