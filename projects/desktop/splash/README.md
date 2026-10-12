# mlx-splash

The boot splash of the MLX system, in place of Plymouth: the MLX letters
in a night of indigo while the system starts, powers off or restarts.
It is an atom of MLX. Three rings of light orbit the letters, tilted in
depth so they pass behind them and in front. Their planes swing and rock
(30 degrees either way, once in 12 to 17 s), and bright arcs run round
them. The code graph of the Observatory rides the rings, its modules as
atoms in the icon's colours (orange, green, purple, cyan, yellow): a
glowing core, three electron orbits turning round it and an electron
racing on each. They are linked into triangles and labelled as the
Observatory's map labels them. A dial of ticks with two
thick violet arcs sits behind the letters, and dust drifts out. Under the
letters, what is starting stands in corner brackets over a cyan progress
bar.

![The boot splash at 1280x720](screenshots/boot.png)

It speaks Plymouth's protocol, so systemd tells it what starts as it
would tell plymouthd, and `mlx-splashctl` (or the `plymouth` tool) talks
to it. Everything is Mlx on the kernel's interfaces: KMS through the
compositor's `kms.mlx`, the Linux framebuffer, the Plymouth socket. There
is no libc and no Plymouth library.

## Running it

```
mlx-splash [--mode boot|shutdown|reboot|updates] [--output auto|kms|fbdev|none]
           [--card PATH | --card-fd N] [--framebuffer PATH] [--tty PATH | --no-tty]
           [--socket NAME] [--duration-file PATH] [--ask-password-dir PATH]
           [--pid-file PATH] [--snapshot PNG] [--size WxH] [--fps N]
```

As a daemon (the units start it so) it takes the first card under
`/dev/dri` with a connected monitor, or `/dev/fb0` (32 bits per pixel)
when there is none. On the card it keeps the mode at 60 Hz at most, so
the console's mode most likely stays. With no screen at all it still
answers, drawing nothing. The terminal under it (`--tty`, `/dev/tty1`)
goes into graphics mode, so the kernel's console does not draw over it.
`--output none --snapshot PNG` runs it without a screen and writes the
last frame when it quits (the checks do this).

Without a daemon it draws pictures:

```
mlx-splash --size 1920x1080 --png boot.png --at 4 --status "Starting network" --progress 60
mlx-splash --size 1280x720 --frames DIR --count 90 --fps 30 --mode shutdown
mlx-splash --size 3840x2160 --bench 60          # set-up and frame times
```

## What it is told

| Request | Plymouth's byte | What the splash does |
|---|---|---|
| ping | `P` | answers |
| update | `U` | systemd's unit at boot: "Starting NetworkManager"; when going down: "Stopping ..." |
| change-mode | `C` | boot-up, shutdown, reboot, updates (system-upgrade, firmware-upgrade) |
| system-update | `u` | the bar at that percentage |
| display-message, hide-message | `M`, `m` | a line over the status |
| pause-progress, unpause-progress | `A`, `a` | the boot bar stops and goes on |
| hide-splash, show-splash | `H`, `$` | the text console instead, and back |
| deactivate, reactivate | `D`, `r` | DRM master given to another program (a display manager) and taken back; nothing is drawn in between |
| quit | `Q` | answered once done: the bar filled, faded out, the console back |
| quit --retain-splash | `Q` with 1 | the last frame stays on the screen for the display manager (`DRM_IOCTL_MODE_CLOSEFB`, Linux 6.8) |
| has-active-vt | `V` | yes when there is a terminal under it |
| sysinit, newroot, reload, error | `S`, `R`, `l`, `!` | answered |
| password, cached password, question, keystroke | `*`, `c`, `W`, `K` | refused (NAK) |

Only root's requests are carried out (`SO_PEERCRED`). Others are refused.

The splash never asks for a password. When one is asked on the console
(systemd's `/run/systemd/ask-password` holds a request), the splash steps
aside and shows the console with its prompt. It comes back once the
request is answered.

At boot the bar estimates progress from the last boot's length
(`/var/lib/mlx/splash/boot-duration`, written on quit). It fills most of
the way in that time, then creeps on.

```
mlx-splashctl ping | quit [--retain-splash] | wait | deactivate | reactivate
mlx-splashctl show-splash | hide-splash | message TEXT | hide-message TEXT
mlx-splashctl update TEXT | change-mode MODE | system-update PERCENT
mlx-splashctl pause-progress | unpause-progress | has-active-vt
```

The `plymouth` tool's spellings work too (`--quit`, `--ping`, `--wait`,
`--update=TEXT`, `display-message --text=TEXT`, `change-mode --shutdown`,
`system-update --progress=N`).

## Handing the screen over

Whoever takes the screen ends the splash, as display managers end
plymouthd:

- the MLX compositor on the monitor (`quit --retain-splash`, then its own
  mode set, with no text console in between);
- `mlx-console` on the framebuffer (`quit`, the console back);
- a display manager through the drop-in `mlx-splash.conf`
  (`ExecStartPre=-mlx-splashctl quit --retain-splash`);
- without a display manager, `mlx-splash-quit.service` before the login
  prompt on the first terminal.

A plymouthd started from the initramfs holds the socket when the splash
starts. It is asked to quit with `--retain-splash`, and the splash takes
over. When its card goes away (simpledrm is unplugged as the GPU's driver
loads), the splash looks for a card again every second. It makes the
picture again at the new size, with what it said. In shutdown and reboot
mode its `argv[0]` starts with `@`, which systemd-shutdown's last
killing spares, as it spares plymouthd.

## Installing

```
tools/install_splash.sh [--prefix DIR] [--destdir DIR] [--enable | --disable]
```

It installs `mlx-splash` and `mlx-splashctl`, the units `mlx-splash`
(boot, in `sysinit.target`), `mlx-splash-quit`, `mlx-splash-poweroff`,
`mlx-splash-reboot`, the display manager's drop-in, and the mlx-init
service. `--enable` enables them and masks Plymouth's units, keeping what
was there; `--disable` puts it back. `mlx.splash=0`, `plymouth.enable=0`
or `nosplash` on the kernel's command line keeps the splash away. The
splash shows from the real root, after udev has found the cards; it is
not in the initramfs.

## How it draws

Everything is measured in a sixteenth of the widest 16:9 box the screen
holds, so the picture keeps its proportions at any size. The CPU draws
it into memory, and the animated region is copied to the screen.

- The backdrop is drawn once: a dithered radial gradient, bokeh, stars,
  faint constellations.
- Light is added in packed words: red and blue in one multiply, then a
  saturating add across the channels at once.
- Each frame restores only the animated region from the backdrop.
- Once they have come in (1.75 s), the parts that stand still are drawn
  into the backdrop: the dial and the letters' glow.
- What remains per frame is what moves: the rings, their arcs and glints,
  the atoms, dust and text. Halos are added two pixels at a time in one
  64-bit word, and the far side of a ring has none. A 1920x1080 frame
  takes about 10 ms, 1280x800 about 5 ms.
- It draws 30 frames a second at most. A frame never comes sooner than
  three times the last one's cost, so the splash uses a third of a
  processor at most.

| File | What |
|---|---|
| `paint.mlx` | the surface, packed colour arithmetic, sparks, glows, lines, copies |
| `logo.mlx` | the MLX letters as shaded facets (std.ui.vector) and their glow |
| `state.mlx` | the scene's data, shared by the drawing modules |
| `backdrop.mlx` | the night, and baking the still parts into it |
| `orbits.mlx` | turning rings, arcs, glints, the code graph's atoms, the dial, dust, stars |
| `status.mlx` | the status in brackets, the message, the progress bar |
| `scene.mlx` | setting up and running a frame |
| `output.mlx` | KMS, fbdev or nothing; the terminal's graphics mode |
| `protocol.mlx` | Plymouth's protocol, server and client |
| `daemon.mlx` | the daemon's loop |
| `handover.mlx` | ending the splash from the program taking the screen |
| `tool.mlx` | mlx-splashctl |
| `preview.mlx`, `options.mlx`, `system.mlx`, `main.mlx` | pictures, the command line, system calls |

## Checks

`tools/check_splash.sh` touches none of the machine's cards, framebuffers
or terminals. It covers:

- pictures: `--png`, `--frames` and `--bench`;
- the protocol headless, as systemd and the plymouth tool send it:
  - updates whose answer is never read, a malformed request, the socket
    taken;
  - passwords, questions and keystrokes refused, a non-root sender
    refused;
  - mlx-splashctl's commands, quit answered when done, the boot's length
    kept;
- taking over from a stand-in plymouthd;
- an emulated KMS card (`tools/fake_drm_session.py`):
  - the mode set and page flips, the frame's pixels;
  - stepping aside for a password and coming back;
  - deactivate and reactivate;
  - quit restoring the console, and quit --retain-splash leaving the
    frame with `CLOSEFB`;
  - the card going away.

## Not yet

- No splash in the initramfs: the real root starts it.
- mlx-init starts it at boot only, not at shutdown.
- No themes: the picture is the MLX one.
