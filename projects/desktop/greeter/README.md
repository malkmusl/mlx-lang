# mlx-greeter

`mlx-greeter` is the login surface shown by `mlx-compositor --greeter`.
It is a normal `std.ui.host` Wayland client and follows the desktop's
light/dark appearance. The greeter itself is unprivileged: it reads no
account database and owns no device.

Accounts come from `org.mlx.Login.ListUsers` on the MLXIPC system bus at
`/run/mlx/bus`. `std.dbus` is used only as MLXIPC's in-tree wire codec;
there is no external D-Bus daemon or library. Sessions come from the freedesktop
`wayland-sessions/*.desktop` directories below `XDG_DATA_HOME` and
`XDG_DATA_DIRS`. Only entries whose `Exec` is one executable count; the
command is passed as data to `mlx-login`, never to a shell.

It looks like the lock screen (`projects/desktop/lock`), fullscreen: one
card with the account mark, the account's name, the session choice, the
password and the button. The arrows beside the mark go through the
people's accounts; root and accounts without a login shell are not among
them. A click on
the name turns it into a field for any account name, root's too (Escape
or an arrow goes back). The session row below the account has arrows for
all valid `wayland-sessions` entries. It initially chooses the entry
running `mlx-session` (or the first entry when that is not installed),
and sends the executable belonging to the visibly selected name to
`mlx-login`.

The password field displays stars. On every accepted, rejected, failed,
or closed attempt, the entire fixed password buffer and its display copy
are overwritten. `mlx-login` performs authentication and session setup;
after a successful `Login` reply the greeter closes so its compositor can
release the seat.

```sh
mlx4 projects/desktop/greeter/main.mlx -o mlx-greeter
tools/check_greeter.sh
```
