# sentinel

The desktop's login service, in Mlx: the replacement for a display
manager (SDDM, GDM) and for the part of systemd-logind that starts a
user's session. It runs as root, owns `org.mlx.Login` and the compositor's
small `org.freedesktop.login1` subset on the MLXIPC system bus
(`synapse --system`), and the greeter (the compositor in
greeter mode, with `aurora`) asks it to log a person in. No PAM, no
logind, no libc.

```sh
mlx4 projects/desktop/login/main.mlx -o sentinel
synapse --system &                          # the system bus at /run/mlx/bus
sudo sentinel --verbose                     # owns org.mlx.Login, waits
sudo sentinel --passwd FILE --shadow FILE   # isolated account databases
sudo sentinel --greeter kepler aurora
tools/check_login.sh                         # the end-to-end check
```

The default account-database paths are `/etc/passwd`, `/etc/shadow` and
`/etc/group`; session runtime directories live below `/run/mlx/user`.
The options exist so installers and checks can stage private databases;
`--group FILE` and `--runtime-dir DIR` isolate the remaining session data.
`--tty FILE` selects the VT control device (`/dev/tty0` by default; `none`
is the explicit namespace-test backend). `--test-device-null` only adds
`/dev/null` to the otherwise DRM/input-only device allowlist used by the
namespace check.
They do not remove the root requirement. The check re-executes itself as
root inside a user namespace and passes private fixture paths explicitly,
so `sentinel` does not open the host's account databases.

`--greeter COMPOSITOR PROGRAM` makes the daemon own the display-manager
lifecycle. With no user session it starts `COMPOSITOR --greeter PROGRAM`,
drops that child to uid/gid 65534, and gives it a private mode-0700
runtime/home directory. A successful login first revokes compositor
control and stops that greeter; when the user session exits, a fresh one
is started. `--greeter-uid` and `--greeter-gid` select another dedicated
numeric identity when a system reserves one. The installed units enable
this mode and expect `kepler` from the compositor installation in
the same `PREFIX/bin/mlx` (`/usr/bin/mlx` by default, on PATH after the
installation: [`tools/lib/mlx_layout.sh`](../../../tools/lib/mlx_layout.sh)).

## Installation

`tools/install_login.sh` builds and installs `sentinel`, `aurora`
and `umbra`, the `sentinel.service` systemd unit, and an impulse boot
set for `synapse`, `nexus` and `sentinel`. Installation alone does
not change the running display manager:

```sh
tools/install_login.sh --destdir "$PWD/stage" # inspect a package tree
sudo tools/install_login.sh                    # install, do not enable
sudo tools/install_login.sh --enable           # switch display managers
sudo tools/install_login.sh --disable          # restore the prior one
tools/check_install_login.sh                   # staging and rollback check
```

`--enable` makes `sentinel.service` both the graphical target's service
and the `display-manager.service` alias. Before doing that, it records the
previous alias, the previous manager's local unit override or mask, and a
pre-existing graphical-target link below
`/var/lib/mlx/login-installer`. It then masks only the manager selected by
the old alias. Repeating `--enable` keeps the first snapshot; `--disable`
restores it exactly and is itself safe to repeat. With `--destdir`, all
links and saved state remain inside the staged tree and no `systemctl`
command is run.

## The bus interface (`org.mlx.Login`, path `/org/mlx/Login`)

| Method | What it does |
| --- | --- |
| `ListUsers() -> a(ssss)` | the login accounts of the configured passwd file: name, full name, home, shell |
| `Login(s user, s password, s session) -> b` | checks the password, and on a match starts `session` as `user` |
| `Unlock(s user, s password) -> b` | checks the password of the user who already holds the seat (the lock screen), for a caller who is that user |

`ListUsers` reads the configured passwd file, `/etc/passwd` by default;
that file is world-readable and holds no secrets. The greeter shows that
list, takes a password, and calls `Login`. A wrong password is held back
for two seconds before the "no", to slow guessing.

`Unlock` does not take the caller's word for who asks. After MLXIPC
allowed `login.session`, sentinel asks the bus which user the calling
connection is (`GetConnectionUnixUser`: what the kernel told the bus
when the caller connected, the channel's peer on brixOS, the socket's
credentials on Linux). The password is checked only when that user holds
the seat and `user` names them; root may ask for them. Anyone else gets
the delayed "no", and no password is checked for them. `--verbose` says
`login: unlock allowed`, `login: unlock refused`, or that the caller is
not the session's user.

## The two privileged steps

Two small modules hold the only operations that need root's power: reading
a password hash and changing the session process's identity.

The pieces the two steps need are all in the tree already:

- **`authenticate`** reads the configured shadow file (`/etc/shadow` by
  default, root only) without following a final symlink, validates that it
  is a root-owned regular file not writable by group or others, and calls
  `std.crypt.verify(password, hash)` — [`std/src/crypt.mlx`](../../../std/src/crypt.mlx),
  which already does yescrypt (`$y$`, Ubuntu's default), `$6$` and `$5$`
  in constant time. Empty, locked, malformed and unreadable records fail
  closed; password and hash buffers are wiped on every path.
- **`startSession`** forks, becomes a session leader, drops the
  supplementary groups, the gid and the uid in that order, builds the
  session's explicit environment, changes to its home and uses
  `projects/desktop/ipc/activation.mlx`'s `execute` for the PATH lookup.
  A close-on-exec pipe prevents a failed privilege drop or exec from being
  reported as a successful login.

## The seat

A session's compositor gets the VT, the DRM card and the input devices
the way it already asks for them: its logind client
([`projects/desktop/compositor/logind.mlx`](../compositor/logind.mlx))
calls `org.freedesktop.login1` on the system bus for `TakeControl`,
`TakeDevice`, `ReleaseDevice`, `PauseDeviceComplete` and `SwitchTo`.
`sentinel` answers those calls for its one `seat0` session, passes
character-device descriptors with `SCM_RIGHTS`, and sends `PauseDevice`
before leaving the session VT. A small `VT_WAITACTIVE` child wakes the
daemon when the kernel really returns to that VT; only then does it send
`ResumeDevice` with fresh descriptors. Device requests are limited to
Linux DRM and input majors. `TakeControl` is asynchronous and
requires MLXIPC's `login.device` permission, which defaults only to
`kepler`; a session generation prevents a delayed permission reply
from crossing a logout/login boundary. `NameOwnerChanged` releases a
controller that disconnected. A signalfd reaps both the VT watcher and the
session without waiting for new bus traffic, releases the seat, and leaves
it ready for a new greeter.

The same MLXIPC service exposes the login1 manager's power surface:

| Method | Result |
| --- | --- |
| `CanPowerOff() -> s` | `yes` while impulse's control socket accepts connections, otherwise `no` |
| `CanReboot() -> s` | `yes` while impulse's control socket accepts connections, otherwise `no` |
| `PowerOff(b interactive)` | asks MLXIPC for `login.power`, then sends `poweroff` to impulse |
| `Reboot(b interactive)` | asks MLXIPC for `login.power`, then sends `reboot` to impulse |

These are D-Bus-compatible method names and encodings carried by MLXIPC;
they do not contact dbus-daemon or systemd. `PowerOff` and `Reboot` work
from the greeter as well as from a logged-in session and always require
the caller's `login.power` permission. The boolean is accepted for login1
compatibility; MLXIPC makes the authorization decision. impulse supplies
its root-only control path in `MLX_INIT_CONTROL`; without that environment
entry the login service uses `/run/mlx/init`. A missing, stale or refusing
socket produces a D-Bus `Failed` error instead of attempting a kernel
reboot directly.

## Restarting and shutting down

The desktop's own way to end the system is on `org.mlx.Login`
([`power.mlx`](power.mlx)), in MLXIPC's native format; the login1 methods
above are the surface for foreign software, and on brixOS they reach
nothing (they speak to impulse's Linux socket).

| Method | What it does |
| --- | --- |
| `Restart() -> b` | the machine is restarted |
| `PowerOff() -> b` | the machine is shut down |

Who may is decided here, by two questions to the bus about the caller.
The bus answers both from what the kernel says of the connection (its
program and its user: channel_peer on brixOS, the socket's credentials on
Linux), never from what the caller says of itself:

1. `org.mlx.IPC.Check(caller, "login.power")`: the bus's policy for the
   app (an app that is not sandboxed has it unless `ipc.conf` says
   otherwise).
2. `org.freedesktop.DBus.GetConnectionUnixUser(caller)`: the caller's user
   must be the one who holds the seat: the person logged in, or the
   greeter's user while nobody is.

A program of another user (root's shell on a serial console too: root has
`impulsectl`), and any program whose user has no session on the seat, gets
`AccessDenied` and nothing happens; where no init listens (a system
another init runs) every caller gets an error, so that no session ends for
a request nobody takes. An allowed call is answered `true`;
then the seat's compositor is told to end (it leaves its last frame for
the shutdown's screen) and the init is asked on its control
([`projects/init/request.mlx`](../../init/request.mlx)): on brixOS the FIFO
`/temporary/init/control`, which only the system may write, on Linux the
socket `$MLX_INIT_CONTROL`. impulse stops the services and has the kernel
end the machine.

The askers are the session's compositor, for the top bar's session menu
(Restart and Shut Down, after the windows were asked to close), and the
greeter, for the two buttons under its card; both through
[`projects/desktop/shared/system_end.mlx`](../shared/system_end.mlx).
`tools/check_brix_session.sh` goes the whole way in QEMU: the refusal, a
log out, a restart into a second boot, a shut down.

## Files

| File | What it does |
| --- | --- |
| `main.mlx` | the daemon: the bus, authorization and the three methods |
| `authentication.mlx` | bounded, fail-closed shadow parsing and password verification |
| `greeter.mlx` | unprivileged greeter-compositor process lifecycle |
| `groups.mlx` | the primary and supplementary groups for a session |
| `login1.mlx` | one-seat compositor control, device descriptors and VT switching |
| `power.mlx` | Restart and PowerOff: who may (the user who holds the seat), and the request to the init |
| `session.mlx` | runtime directory, privilege drop, environment and exec |
| `users.mlx` | the login accounts, read from the configured passwd file |
| `native.mlx` | brixOS: the children branched, given an identity and replaced |
| `examples/` | systemd and impulse service templates used by the installer |

## On brixOS

The same daemon runs on brixOS (`--target=x86_64-brixos`) through the
native interface, never the Linux layer. There is no signalfd and no
wait4: the wait is the native `wait` on the bus connection and the
greeter's and session's process handles, and the children are started
by branching (process_branch), taking the target identity (identity_set)
in a runtime directory made and owned natively (node_make, node_own),
and replacing the branch with the program (process_replace); a stream
pair closed on the replace reports a failure before it. `native.mlx`
holds these, shared by `greeter.mlx` and `session.mlx`. The bus
(projects/desktop/ipc) runs natively too, and so the policy can tell
aurora apart, the kernel names each channel's holder program
(channel_peer), which std.appid reads. tools/check_brix_login.sh covers
the whole chain. The same call gives the bus each client's user, which
is what Unlock's caller check rests on there; tools/check_brix_lock.sh
covers the lock (umbra) and that check.
