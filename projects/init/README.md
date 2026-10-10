# mlx-init

PID 1 of MLX/Linux, written in Mlx without libc: `mlx-init` mounts the
virtual file systems and what the mounts file lists, sets the hostname,
starts the services defined in a directory in their order, supervises
them (ended ones start again after a pause that grows while they keep
failing), answers `mlx-initctl` over a control socket, and shuts the
system down in the reverse order. Not PID 1 (started by hand, or by
`tools/check_init.sh`), it supervises the same way without the mounts,
the hostname and the reboot.

```sh
mlx4 projects/init/main.mlx -o mlx-init
mlx4 projects/init/tool.mlx -o mlx-initctl
tools/check_init.sh                      # the end-to-end check
```

As the system's init: install it as `/sbin/init` (or boot with
`init=/usr/bin/mlx-init`), the services into `/etc/mlx/init/services/`,
the mounts into `/etc/mlx/init/mounts`; `examples/` holds a start
(the MLXIPC system bus, a shell on the console, a oneshot).

## What happens at boot

1. `/proc`, `/sys`, `/dev` (devtmpfs), `/dev/pts`, `/dev/shm` and `/run`
   are mounted where no file system of that type is yet (the kernel may
   have mounted some), Ctrl-Alt-Del is turned into a signal, the hostname
   comes from `/etc/hostname`.
2. The mounts file (`--mounts FILE`, default `/etc/mlx/init/mounts`) is
   applied: fstab's first four fields per line, `SOURCE TARGET TYPE
   OPTIONS`; the usual options (`ro`, `nosuid`, `nodev`, `noexec`,
   `noatime`, `relatime`, `bind`, ...) become mount flags, `noauto` skips
   a line, swap and `none` are left alone, and anything else goes to the
   file system (`mode=1777`). A mount that fails is said and does not
   stop the boot.
3. The services directory (`--services DIR`, default
   `/etc/mlx/init/services`) is read, and every service not `disabled`
   starts as soon as the services it names in `after` run.
4. The init listens at the control socket (`--control PATH`, default
   `/run/mlx/init`, root only) and supervises.

## Services

`NAME.service`, a few `key = value` lines (`#` comments):

```
description = MLXIPC system bus
exec = /usr/bin/mlx-ipcd --system        # the command line; double quotes
                                         # keep spaces in an argument; a
                                         # program without a slash is looked
                                         # up along the standard PATH
after = mounts hostname                  # start once these run (a oneshot:
                                         # finished)
ready = /run/mlx/bus                     # running once this path exists
                                         # (else right after the start);
                                         # ready-timeout = 30s at the latest
type = simple | oneshot                  # a oneshot runs to its end once
restart = always | on-failure | no       # default always (a oneshot: no)
stop-timeout = 10s                       # SIGTERM, then SIGKILL after this
log = /var/log/NAME.log | console        # default LOGDIR/NAME.log
stdio = /dev/tty1                        # a terminal as stdin, stdout and
                                         # stderr (a shell on the console)
env = KEY=VALUE                          # more environment (repeatable)
disabled = yes                           # defined, not started at boot
```

Every service runs in a session and process group of its own, as root,
with `PATH`, `HOME=/`, `TERM=linux`, `MLX_INIT_CONTROL` (the control
socket) and its `env` lines as environment, its stdout and stderr in its
log (`--log DIR`, default `/run/mlx/log`) unless `log` or `stdio` say
otherwise. A service that ends is started again after 1 s; when it ran
less than a minute, the pause doubles each time up to 30 s. The states
`mlx-initctl status` shows: `waiting` (for `after`), `starting` (for
`ready`, or a oneshot running), `running`, `done` (a oneshot finished),
`stopping`, `restarting` (in its pause), `stopped`, `failed` (ended with
a status and not started again), `disabled`.

## mlx-initctl

```
mlx-initctl status                       # one line per service
mlx-initctl start NAME | stop NAME | restart NAME
mlx-initctl reload                       # the services directory again:
                                         # added services start, removed
                                         # ones stop
mlx-initctl poweroff | reboot | halt
```

The socket is `/run/mlx/init`, `--control PATH`, or `$MLX_INIT_CONTROL`.
The protocol is one request line and a text reply ending in `ok` or
`error: WHY` (control.mlx).

## Signals and the shutdown

SIGCHLD reaps (the services' processes, and orphans that came to PID 1),
SIGHUP reads the services again, SIGINT (Ctrl-Alt-Del) and SIGTERM
reboot, SIGUSR1 halts, SIGUSR2 powers off; not PID 1, each of the four
ends the init. The shutdown (power.mlx) sends SIGTERM to every service's
process group in the reverse order of their starts and waits for them
(SIGKILL after each one's stop-timeout, a minute at most), then SIGTERM
and SIGKILL to every process left, syncs, unmounts everything but the
root and the virtual file systems, makes the root read-only, and asks
the kernel to power off, reboot or halt.

## The modules

- `main.mlx`: the options, the boot steps, the event loop (epoll over a
  signalfd, the control socket and its clients), `std.crash` so a crash
  of PID 1 says where it was.
- `config.mlx`: the service records and their state, the directory and
  the `key = value` files, the init's own messages.
- `supervise.mlx`: starting (fork, session, stdio, PATH lookup, execve),
  reaping, the pauses, readiness, stopping, the stop for the shutdown.
- `mounts.mlx`: the virtual file systems, the mounts file, the unmounting.
- `control.mlx`: the control socket and the requests.
- `power.mlx`: the shutdown.
- `linux.mlx`: the system calls (processes, signals, mounts, reboot, Unix
  sockets, epoll).
- `tool.mlx`: mlx-initctl.

## Checks

`tools/check_init.sh` builds both programs and runs the init as a plain
supervisor (order by `after` and `ready`, a oneshot, a failing service
and its growing pause, a disabled one, stop, start, restart, an unknown
name, reload with an added and a removed service, SIGTERM), then as PID 1
of a user, PID, mount and UTS namespace (`unshare`): the hostname from a
file, a tmpfs from the mounts file, a service looking around, and a
poweroff from `mlx-initctl` that stops the service and ends the
namespace.

## Not yet

Services run as root (no `user =`); readiness is a path, not a socket or
a message; the shutdown stops every service at once (in reverse order,
without waiting for dependents first); logs are files under `/run`
(gone with the reboot) with no rotation; no device events (the kernel's
devtmpfs alone), no network setup, no file system checks, no
`/etc/fstab` beyond its first four fields.
