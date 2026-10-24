# mlx-init

PID 1 of MLX/Linux, written in Mlx without libc: `mlx-init` mounts the
virtual file systems and what the mounts file lists, sets the hostname,
starts the services defined in a directory in their order, supervises
them (ended ones start again after a pause that grows while they keep
failing), answers `mlx-initctl` over a control socket, and shuts the
system down in dependency order. Not PID 1 (started by hand, or by
`tools/check_init.sh`), it supervises the same way without the mounts,
the hostname and the reboot.

```sh
mlx4 projects/init/main.mlx -o mlx-init
mlx4 projects/init/tool.mlx -o mlx-initctl
tools/check_init.sh                      # the end-to-end check
```

As the system's init: install it in `/usr/bin/mlx/mlx-init` and boot a
separate entry with `init=/usr/bin/mlx/mlx-init`; keeping the normal entry
provides a systemd fallback. Put services in `/etc/mlx/init/services/`.
Mounts and checks use the system's `/etc/fstab`; `examples/` holds a start
(the MLXIPC system bus, network, login, the Mlx console with mlx-sh on the
framebuffer, and a oneshot).

`tools/install_login.sh` installs a component-maintained desktop boot set
into that services directory: `mlx-ipcd`, then `mlx-netd`, and
`mlx-login` after both. Its `--destdir` mode stages the same definitions
without touching the host.

## What happens at boot

1. `/proc`, `/sys`, `/dev` (devtmpfs), `/dev/pts`, `/dev/shm` and `/run`
   are mounted where no file system of that type is yet (the kernel may
   have mounted some), Ctrl-Alt-Del is turned into a signal, the hostname
   comes from `/etc/hostname`.
2. `/etc/fstab` (or `--mounts FILE`) is read with all six fields. Entries
   with a nonzero pass are checked synchronously in pass order through
   `/sbin/fsck` (or `--fsck PROGRAM`; `none` disables checks), then mounted.
   The usual options (`ro`, `nosuid`, `nodev`, `noexec`, `noatime`,
   `relatime`, `bind`, ...) become mount flags, `noauto` skips both steps,
   swap and `none` are left alone, and file-system-specific options go to
   the kernel. Fstab octal escapes are decoded, and `UUID=`, `LABEL=`,
   `PARTUUID=` and `PARTLABEL=` sources use their stable `/dev/disk/by-*`
   paths. The initramfs must run its device manager (for example the classic
   mkinitcpio `udev` hook) and preserve `/dev` across `switch_root`, because
   mlx-init deliberately does not duplicate filesystem probing to create
   those links. A check result other than clean or corrected stops the boot,
   syncs and halts before any fstab mount or service is started. A failed
   mount is reported and does not silently count as success.
3. A kobject-uevent Netlink socket is opened before coldplug scans
   `/sys/class/drm`, `/sys/class/input` and `/sys/class/sound`. Existing and
   newly added nodes below `/dev/dri`, `/dev/input` and `/dev/snd` become
   mode `0660` and group `video`, `input` and `audio`, respectively. Group
   numbers come from `/etc/group`; unsafe `DEVNAME` paths are ignored.
4. The services directory (`--services DIR`, default
   `/etc/mlx/init/services`) is read, and every service not `disabled`
   starts as soon as the services it names in `after` run.
5. The init listens at the control socket (`--control PATH`, default
   `/run/mlx/init`, root only) and supervises.

## Services

`NAME.service`, a few `key = value` lines (`#` comments):

```
description = MLXIPC system bus
exec = /usr/bin/mlx/mlx-ipcd --system    # the command line; double quotes
                                         # keep spaces in an argument; a
                                         # program without a slash is looked
                                         # up along the standard PATH and
                                         # /usr/bin/mlx
after = mounts hostname                  # start once these run (a oneshot:
                                         # finished)
ready = /run/mlx/bus                     # running once this path exists
ready = socket:/run/mlx/bus              # alternatively, only once a Unix
                                         # stream connection succeeds
                                         # (else right after the start);
                                         # ready-timeout = 30s at the latest
type = simple | oneshot                  # a oneshot runs to its end once
restart = always | on-failure | no       # default always (a oneshot: no)
stop-timeout = 10s                       # SIGTERM, then SIGKILL after this
log = /var/log/NAME.log | console        # default LOGDIR/NAME.log
stdio = /dev/tty1                        # a terminal as stdin, stdout and
                                         # stderr (a shell on the console)
env = KEY=VALUE                          # more environment (repeatable)
user = NAME                              # account to run as
group = NAME                             # optional group override; otherwise
                                         # the user's primary group
disabled = yes                           # defined, not started at boot
```

Every service runs in a session and process group of its own. Without
`user`/`group` it keeps the init's identity; with them, mlx-init resolves
`/etc/passwd` and `/etc/group`, clears root's supplementary groups, then
applies the requested gid and uid before `execve`. Unknown accounts and
privilege changes that cannot be made are refused instead of silently
running under the wrong identity. `HOME`, `USER` and `LOGNAME` follow the
selected user, alongside `PATH`, `TERM=linux`, `MLX_INIT_CONTROL` and the
service's `env` lines.

Stdout and stderr go through a small relay unless `log = console` or
`stdio` selects a terminal. The persistent default is
`/var/log/mlx/NAME.log`; while that file system is unavailable the relay
uses `/run/mlx/log/NAME.log`, then copies the early records and switches
the live service as soon as persistent storage opens. Logs rotate at 1 MiB
with four backups. `--log DIR`, `--runtime-log DIR` and `--log-size BYTES`
override those values (the latter two are also useful in tests).

A service that ends is started again after 1 s; when it ran less than a
minute, the pause doubles each time up to 30 s. The states
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
ends the init. The shutdown (power.mlx) first stops services that depend on
others through `after`, waits for each one (including its log relay), and
only then stops its dependencies. A dependency cycle falls back to reverse
start order. SIGKILL follows each service's stop timeout, with a minute for
the service set as a whole; the init then sends SIGTERM
and SIGKILL to every process left, syncs, unmounts everything but the
root and the virtual file systems, makes the root read-only, and asks
the kernel to power off, reboot or halt.

## The modules

- `main.mlx`: the options, the boot steps, the event loop (epoll over a
  signalfd, the control socket and its clients), `std.crash` so a crash
  of PID 1 says where it was.
- `config.mlx`: the service records and their state, the directory and
  the `key = value` files, the init's own messages.
- `accounts.mlx`: direct passwd/group parsing for service identities.
- `supervise.mlx`: starting (fork, session, stdio, PATH lookup, execve),
  reaping, the pauses, readiness, stopping, the stop for the shutdown.
- `logs.mlx`: live persistent-log promotion and bounded rotation.
- `mounts.mlx`: the virtual file systems, the mounts file, the unmounting.
- `devices.mlx`: coldplug from sysfs, hotplug from kernel Netlink and the
  desktop device classes' ownership and permissions.
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
poweroff from `mlx-initctl` that stops the service and ends the namespace.
It also runs focused checks with a synthetic sysfs/dev tree and verifies
that socket readiness waits for a listener rather than mere path existence,
that user/group are applied, that dependents finish first during shutdown,
that a live log moves to persistent storage and rotates, and that all six
fstab fields, escapes, skip rules and fsck pass ordering are honored.

## Boundary

No network setup is built into init itself; networking is a supervised
`mlx-netd` service.
