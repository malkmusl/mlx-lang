# mlx-login

The desktop's login service, in Mlx: the replacement for a display
manager (SDDM, GDM) and for the part of systemd-logind that starts a
user's session. It runs as root, owns `org.mlx.Login` on the MLXIPC
system bus (`mlx-ipcd --system`), and the greeter (the compositor in
greeter mode, with `mlx-greeter`) asks it to log a person in. No PAM, no
logind, no libc.

```sh
mlx4 projects/desktop/login/main.mlx -o mlx-login
mlx-ipcd --system &                 # the system bus at /run/mlx/bus
sudo mlx-login --verbose            # owns org.mlx.Login, waits
tools/check_login.sh                # the end-to-end check
```

## The bus interface (`org.mlx.Login`, path `/org/mlx/Login`)

| Method | What it does |
| --- | --- |
| `ListUsers() -> a(ssss)` | the login accounts of `/etc/passwd`: name, full name, home, shell |
| `Login(s user, s password, s session) -> b` | checks the password, and on a match starts `session` as `user` |
| `Unlock(s user, s password) -> b` | checks the password of the user who already holds the seat (the lock screen) |

`ListUsers` reads `/etc/passwd`, which is world-readable and holds no
secrets. The greeter shows that list, takes a password, and calls
`Login`. A wrong password is held back for two seconds before the "no",
to slow guessing.

## The two privileged steps

Two steps need root's power, and they are the only ones that read a
password hash or change the process's user. They are kept apart in
`main.mlx` as the functions **`authenticate`** and **`startSession`**,
each a stub that returns `NOT_CONFIGURED` with the full recipe in its
comment. Everything else — the bus, the account list, the greeter
protocol, the failure delay — is finished and runs: build `mlx-login`
today and `ListUsers` answers, while `Login` and `Unlock` reply with
`org.mlx.Login.Error.NotConfigured` until the two are filled in.

The pieces the two steps need are all in the tree already:

- **`authenticate`** reads `/etc/shadow` (root only) and calls
  `std.crypt.verify(password, hash)` — [`std/src/crypt.mlx`](../../../std/src/crypt.mlx),
  which already does yescrypt (`$y$`, Ubuntu's default), `$6$` and `$5$`
  in constant time. The stub's comment gives the field layout and the
  locked-account cases.
- **`startSession`** forks, becomes a session leader, drops the
  supplementary groups, the gid and the uid in that order, builds the
  session's environment, and execve's the session. The stub's comment
  names each syscall and `projects/desktop/ipc/activation.mlx`'s
  `execute` for the PATH lookup.

They are left to whoever runs this on their own machine because reading
the shadow file and changing user are exactly the operations a system's
owner should put in by hand, not inherit from a scaffold.

## The seat

A session's compositor gets the VT, the DRM card and the input devices
the way it already asks for them: its logind client
([`projects/desktop/compositor/logind.mlx`](../compositor/logind.mlx))
calls `org.freedesktop.login1` on the system bus for `TakeControl`,
`TakeDevice` and VT switching. A fuller `mlx-login` answers that
interface too; until then the session's compositor opens its VT directly,
as it does now when no logind is present.

## Files

| File | What it does |
| --- | --- |
| `main.mlx` | the daemon: the bus, the three methods, the two marked stubs |
| `users.mlx` | the login accounts, read from `/etc/passwd` |
