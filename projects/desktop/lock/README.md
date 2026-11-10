# umbra

`umbra` is the Mlx desktop's screen locker. It uses
`ext-session-lock-v1`, so the compositor hides every other surface and
routes input only to the locker until authentication succeeds.

```sh
mlx4 projects/desktop/lock/main.mlx -o umbra
umbra
tools/check_lock.sh
```

The password is displayed only as stars and is sent as
`Unlock(s user, s password) -> b` to `org.mlx.Login` at `/org/mlx/Login`
on the MLXIPC system bus, in MLXIPC's own native format (`std.ipc`), as
the greeter's requests are: native first. The bus (`synapse`) carries it
to the login service whichever format that speaks; where the system bus
is a foreign dbus-daemon, `std.ipc` falls back to D-Bus's format by
itself. There is no dbus-daemon or libdbus dependency. The bus is
selected through `DBUS_SYSTEM_BUS_ADDRESS=unix:path=...` (sentinel gives
the session the bus it is on), or `/run/mlx/bus` by default.
Only an explicit `true` reply ends the Wayland session lock; a denial, a
malformed reply, a timeout, or an unavailable service leaves the screen
locked.

Whose reply counts: the bus carries a reply nobody asked for when its
rules let the sender talk to the receiver, which by default they do, so
any program on the bus can send umbra a `true` with the serial its call
will have. Before each `Unlock` umbra asks the bus which connection owns
`org.mlx.Login` (on the system bus only root owns a name), and takes
that connection's reply alone; the bus stamps every message with its
real sender. A reply from anyone else is passed over.

The user name sent and shown comes from `USER`, which sentinel sets for
the session. It decides nothing by itself: the bus tells sentinel which
user the caller is (the kernel's word: the channel's peer on brixOS, the
socket's credentials on Linux), and sentinel checks the password only
when that user holds the seat and is the one named. A wrong `USER` keeps
the screen locked; it cannot unlock for someone else.

The mutable password field, the outgoing message body and the pages the
message was built in and queued on are overwritten before the client
waits for the login service's reply. The
Wayland keyboard text buffers and the star field are cleared as well, and
logs contain only generic state changes. Error text follows the desktop's
German or English language, while colours follow `std.ui.appearance`
during the lock.

`tools/check_lock.sh` runs the client in the nested Mlx compositor under
`tools/wayland-test-host`. Its private MLXIPC bus and
`tools/lock-login-test` reject one password and accept the next, proving
that denial keeps the lock, success releases it, neither password reaches
a log, and an appearance change repaints the full-screen surface. While
the wrong password is asked, `tools/lock-login-test/forger.mlx` sends
umbra replies `true` from another connection: the lock stays. The
bus's log shows umbra as a native (`mlx`) client and the stand-in service
as a D-Bus one, the bus between them. A small direct client also verifies
that the field, message, and transport pages contain only zeroes after an
accepted request.

## On brixOS

The same program runs on brixOS (`--target=x86_64-brixos`) and is in the
boot image with its app entry: Super+L (or the top bar's session menu)
has the compositor start it with process_spawn; its connection to the
compositor is a channel, its bus the listener node sentinel names in the
session's environment, the wire native. `tools/check_brix_lock.sh` boots
the image in QEMU, logs in through the greeter, locks with Super+L, sees
the card on the display, types a wrong password while another user's
program answers `true` in sentinel's place (still locked, the card says
so) and the right one (the desktop is back), has another user's
process ask with the right password (refused), and finds neither
password in the kernel's, the bus's, sentinel's or the compositor's log.

If umbra ends while it holds the lock (a crash), the session stays
locked and the screen shows no window. brixOS has no other terminal to
start a locker from, so the compositor keeps one hotkey then: Super+L
starts umbra again. The check has a stand-in locker die and unlocks
through a second umbra.

## Files

| File | Purpose |
| --- | --- |
| `main.mlx` | Lock surface, masked input, localized drawing, and lock lifecycle |
| `login.mlx` | Fail-closed `org.mlx.Login.Unlock` client and secret-buffer wiping |
