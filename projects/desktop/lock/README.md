# mlx-lock

`mlx-lock` is the Mlx desktop's screen locker. It uses
`ext-session-lock-v1`, so the compositor hides every other surface and
routes input only to the locker until authentication succeeds.

```sh
mlx4 projects/desktop/lock/main.mlx -o mlx-lock
mlx-lock
tools/check_lock.sh
```

The user name comes from `USER`. The password is displayed only as stars
and is sent as `Unlock(s user, s password) -> b` to `org.mlx.Login` at
`/org/mlx/Login` on the MLXIPC system bus. There is no dbus-daemon or
libdbus dependency; `std.dbus` is only the in-tree wire codec used on the
MLXIPC socket. The bus is selected through
`DBUS_SYSTEM_BUS_ADDRESS=unix:path=...`, or `/run/mlx/bus` by default.
Only an explicit `true` reply ends the Wayland session lock; a denial, a
malformed reply, a timeout, or an unavailable service leaves the screen
locked.

The mutable password field and the complete outgoing message buffer are
overwritten before the client waits for the login service's reply. The
Wayland keyboard text buffers and the star field are cleared as well, and
logs contain only generic state changes. Error text follows the desktop's
German or English language, while colours follow `std.ui.appearance`
during the lock.

`tools/check_lock.sh` runs the client in the nested Mlx compositor under
`tools/wayland-test-host`. Its private MLXIPC bus and
`tools/lock-login-test` reject one password and accept the next, proving
that denial keeps the lock, success releases it, neither password reaches
a log, and an appearance change repaints the full-screen surface. A small
direct client also verifies that the field, message, and transport pages
contain only zeroes after an accepted request.

## Files

| File | Purpose |
| --- | --- |
| `main.mlx` | Lock surface, masked input, localized drawing, and lock lifecycle |
| `login.mlx` | Fail-closed `org.mlx.Login.Unlock` client and secret-buffer wiping |
