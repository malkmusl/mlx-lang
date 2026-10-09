# MLXIPC

MLXIPC is the desktop's message bus: `mlx-ipcd`, a session bus that D-Bus
programs use as they would dbus-daemon, with what the D-Bus session bus
lacks: permissions per app, permissions services ask about, and direct
channels between two apps past the bus. It is written in Mlx on
`std.dbus` (no libc, no libdbus). MLX Audio (`projects/desktop/audio`,
the PipeWire replacement) runs on it.

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
alone may own names, and the policy comes from `/etc/mlx/ipc.conf`.
Programs reach it through `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/run/mlx/bus`
(the compositor's logind client reads that variable). It is where a
display manager of the desktop's own would offer its login and seat
services.

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
  `audio.monitor`, `audio.control` (MLX Audio). Services ask the bus
  (`org.mlx.IPC.Check(s app's connection, s permission) -> b`) before
  they do what it names, so the rules for every service are in one
  place.
- By default an unsandboxed app may do anything but the two permissions
  below (as on any session bus), and a sandboxed one may own its own
  names (its id and the names under it), talk to and see those, other
  connections of the same app, the portals (`org.freedesktop.portal.*`),
  `org.freedesktop.Notifications` and MLX Audio (`org.mlx.Audio`), and
  use `audio.play`; it sees nothing else, not even the other connections'
  unique names. Replies to calls an app made always come back; a reply
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
```

## Modules

| File | What it does |
| --- | --- |
| `main.mlx` | mlx-ipcd: options, listening, signals (signalfd), the policy's directory (inotify), the event loop (epoll, std.turns), `--run` |
| `state.mlx` | the records: bus, clients, names, match rules, waiting replies, policy, activation |
| `bus.mlx` | clients, authentication, names, match rules, routing, monitors |
| `driver.mlx` | what the bus answers itself (org.freedesktop.DBus, org.mlx.IPC) and its signals |
| `policy.mlx` | who a connection is, the rules file, the decisions, rules written back (`setRule`) |
| `prompts.mlx` | asking the user through the permission agent, answers for the session, the apps that asked, ListPermissions and SetPermission |
| `activation.mlx` | `.service` files, starting services, calls waiting for them |
| `tool.mlx` | mlx-ipc |

## Next

- A native MLXIPC protocol next to D-Bus: typed messages (as hyprwire),
  discovery on the bus, the traffic over direct channels.
- polkit: the same agent asks for the system's actions (README of
  `projects/desktop/permissions`).
