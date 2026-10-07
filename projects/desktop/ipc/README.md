# MLXIPC

MLXIPC is the desktop's message bus: `mlx-ipcd`, a session bus that D-Bus
programs use as they would dbus-daemon, with what the D-Bus session bus
lacks: permissions per app, and direct channels between two apps past
the bus. It is written in Mlx on `std.dbus` (no libc, no libdbus) and is
the base MLX Audio (the PipeWire replacement) will build on.

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
Spawn.ChildExited, or 25 seconds went: Spawn.Timeout).

dbus-send, gdbus, busctl and dbus-monitor are checked against it
(`tools/check_ipc.sh`).

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
- By default an unsandboxed app may do anything (as on any session bus),
  and a sandboxed one may own its own names (its id and the names under
  it), talk to and see those, other connections of the same app, the
  portals (`org.freedesktop.portal.*`) and
  `org.freedesktop.Notifications`; it sees nothing else, not even the
  other connections' unique names. Replies to calls an app made always
  come back; a reply nobody asked for needs the rules.
- `~/.config/mlx/ipc.conf` (`--policy FILE`) adds rules, read again when
  it changes (and on SIGHUP, ReloadConfig, `mlx-ipc reload`). The last
  rule that applies wins:

```
# MLXIPC permissions
group media obs com.obsproject.Studio
allow @media talk,see org.mlx.Audio
allow org.mozilla.firefox talk org.freedesktop.secrets
deny firefox talk org.freedesktop.secrets
deny @sandboxed own *
strict            # unsandboxed apps get nothing by default either
```

Subjects: an app, `@GROUP`, `@sandboxed`, `@unsandboxed` or `@all`.
Names: a name, `prefix.*` (prefix and the names under it), `*` or `self`.
A refused call gets `org.freedesktop.DBus.Error.AccessDenied`.

## Direct channels

`org.mlx.IPC.Connect(s NAME) -> h` takes two apps past the bus, the way
hyprtavern's bus is only for finding each other: the bus makes a socket
pair, hands one end to the caller and the other to NAME's owner (a call
`org.mlx.IPC.Peer.Connected(h channel, s caller, s app)` that wants no
reply), and from then on the two talk directly, in D-Bus messages or a
protocol of their own; the bus never sees that traffic. The policy
decides as for a call: the caller must be allowed to talk to NAME. MLX
Audio's streams will go this way (and their buffers as memfds through
it).

`org.mlx.IPC` also answers GetAppId(s) -> (s app, b sandboxed),
ListApps() -> a(ssbu) and Reload().

## mlx-ipc

```
mlx-ipc apps               the connections: unique name, app, sandboxed, process
mlx-ipc names              the names on the bus
mlx-ipc reload             the bus reads its policy again
mlx-ipc serve NAME         owns NAME, answers org.mlx.Echo.Echo(s)
mlx-ipc call NAME TEXT     calls NAME's org.mlx.Echo.Echo(TEXT)
mlx-ipc connect NAME       a direct channel to NAME; prints what comes through
```

## Modules

| File | What it does |
| --- | --- |
| `main.mlx` | mlx-ipcd: options, listening, signals (signalfd), the policy's directory (inotify), the event loop (epoll, std.turns), `--run` |
| `state.mlx` | the records: bus, clients, names, match rules, waiting replies, policy, activation |
| `bus.mlx` | clients, authentication, names, match rules, routing, monitors |
| `driver.mlx` | what the bus answers itself (org.freedesktop.DBus, org.mlx.IPC) and its signals |
| `policy.mlx` | who a connection is, the rules file, the decisions |
| `activation.mlx` | `.service` files, starting services, calls waiting for them |
| `tool.mlx` | mlx-ipc |

## Next

- A native MLXIPC protocol next to D-Bus: typed messages (as hyprwire),
  discovery on the bus, the traffic over direct channels.
- Permission prompts: an app asks, the user answers once in a dialog of
  the desktop's, and the answer goes into `ipc.conf`; the rules in
  mlx-settings.
- MLX Audio on it: the audio server as a bus name, streams over direct
  channels.
