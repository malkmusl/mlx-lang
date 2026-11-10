# ignition

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
would tell plymouthd, and `ignitionctl` (or the `plymouth` tool) talks
to it. Everything is Mlx on the kernel's interfaces: KMS through the
compositor's `kms.mlx`, the Linux framebuffer, the Plymouth socket. There
is no libc and no Plymouth library. Rendering is deliberately CPU-to-KMS:
it needs no Vulkan context, Mesa userspace or render node during early
boot. The initramfs loads the KMS driver and the splash writes dumb
buffers directly.

## Running it

```
ignition [--mode boot|shutdown|reboot|updates] [--output auto|kms|fbdev|none]
           [--card PATH | --card-fd N] [--framebuffer PATH] [--tty PATH | --no-tty]
           [--socket NAME] [--duration-file PATH] [--ask-password-dir PATH]
           [--pid-file PATH] [--snapshot PNG] [--size WxH] [--fps N] [--threads N]
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
ignition --size 1920x1080 --png boot.png --at 4 --status "Starting network" --progress 60
ignition --size 1280x720 --frames DIR --count 90 --fps 30 --mode shutdown
ignition --size 800x450 --frames DIR --fps 25 --story   # a whole boot, start to quit
ignition --size 3840x2160 --bench 60          # set-up and frame times
```

`--story` plays a boot as a script would tell it: units starting every
0.3 s, the bar filling, the splash told to quit at 8.8 s and fading out
(9.4 s of frames). `--threads N` draws with N threads (default: one for
each processor).

## On brixOS

`brixos.mlx` is the splash as brixOS's boot screen (mlx-out/bin/brix/
ignition, in the boot image): the same picture (scene.mlx and the
drawing modules, one thread), into /devices/framebuffer (the geometry
read, the pixels mapped by node_map; the changed tiles copied, red and
blue swapped when the framebuffer wants them so). There is no socket: the
init starts it on two streams and sends Plymouth's requests on its
input ("U" a service starting, "u" the progress in percent, "C", "M",
"m", "Q" with its first argument byte set to keep the picture), each
answered with ACK or NAK on its output; when the input ends, the splash
quits. Told to quit keeping its picture, it writes its last frame to
/temporary/screen-picture (`picture.mlx`: a header and the pixels, from
the frame's own memory; the init removes one an earlier boot left) for
the program that takes the screen: the
compositor fades in from it and removes it, the console removes it;
nobody reads the framebuffer back. The init starts it again in shutdown
or reboot mode when the system goes down, and `impulsectl message TEXT`
reaches it as "M".
What it is told it notes on its errors (the serial console). `--mode`,
`--fps N`, `--snapshot PNG` (the last frame when it quits, as drawn) and
`--screen WIDTH:HEIGHT` are its options. projects/init/brixos.mlx says
when it is shown; tools/check_brix_init.sh looks at the screen.

The picture is drawn for the screen's shape, not the display mode's. A
card's firmware often has no mode of a wide monitor's shape (1280x960 is
the nearest for a 3840x1080 monitor); the monitor stretches the mode it
gets across the screen, and rings drawn round are ellipses, the letters
nearly three times as wide as meant. /devices/framebuffer says the
screen's shape with its geometry (the monitor's own mode, else the mode
the image names; `--screen 32:9` says another). Where the screen is wider
in shape than the mode, the scene is as many pixels across as give a
picture of the mode's height that shape (3413 for 1280x960 on 32:9; four
times the mode's width at most), and each frame goes out brought to the
mode's width (`paint.squeezeOut`: a pixel out is the mean of the stretch
of the row it stands for; only the changed tiles' runs, as before). The
picture left for the next program is the one the display holds. With the
monitor's own mode set (the card's driver, one day) the shapes are one
and nothing is brought together. tools/check_brix_shape.sh measures the
letters on the screen; tests/453_splash_squeeze_runtime.mlx the means.
A screen narrower in shape than its mode is left as it is.

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
ignitionctl ping | quit [--retain-splash] | wait | deactivate | reactivate
ignitionctl show-splash | hide-splash | message TEXT | hide-message TEXT
ignitionctl update TEXT | change-mode MODE | system-update PERCENT
ignitionctl pause-progress | unpause-progress | has-active-vt
```

`ignitionctl absent` is the inverse probe used by the real-root unit:
it succeeds only when an initramfs splash is not already listening.

The `plymouth` tool's spellings work too (`--quit`, `--ping`, `--wait`,
`--update=TEXT`, `display-message --text=TEXT`, `change-mode --shutdown`,
`system-update --progress=N`).

## Handing the screen over

Whoever takes the screen ends the splash, as display managers end
plymouthd:

- the MLX compositor on the monitor (`quit --retain-splash`, then its own
  mode set, with no text console in between);
- `mlx-console` on the framebuffer (`quit`, the console back);
- a display manager through the drop-in `ignition.conf`
  (`ExecStartPre=-mlx-splashctl quit --retain-splash`);
- without a display manager, `ignition-quit.service` before the login
  prompt on the first terminal.

The same ignition process starts in the initramfs and stays through
switch-root. The classic mkinitcpio runscript and the systemd initrd unit
send it a `newroot` request once `/sysroot` is mounted; it chroots there
before the switch, so the initramfs can be released and the boot-duration
file and ask-password directory refer to the real root. The impulse
service is a disabled fallback, and the real-root systemd unit uses the
abstract socket as a condition, so neither starts a duplicate.

The daemon may start before any DRM card exists. It keeps answering and
probes once a second. It does the same when simpledrm disappears as the
GPU's driver loads, then makes the picture again at the new card's size
with the current status. A stray plymouthd on the compatible socket can
still be taken over during migration, but Plymouth is not part of the
normal boot. In shutdown and reboot mode its `argv[0]` starts with `@`,
which systemd-shutdown's last killing spares.

## Installing

```
tools/install_splash.sh [--prefix DIR] [--destdir DIR] [--enable | --disable]
```

It installs `ignition` and `ignitionctl`, the units `ignition`
(boot, in `sysinit.target`), `ignition-quit`, `ignition-poweroff`,
`ignition-reboot`, the display manager's drop-in, the disabled impulse
fallback, and mkinitcpio's `install/ignition` and `hooks/ignition`.
`--enable` enables the real-root and shutdown integration and masks
Plymouth's units, keeping what was there; `--disable` puts those units
back.

Replace `plymouth` with `ignition` after `kms` in
`/etc/mkinitcpio.conf`, then rebuild the images:

```sh
# classic initramfs
HOOKS=(base udev autodetect microcode kms ignition modconf block keyboard keymap consolefont filesystems fsck)

# or a systemd initramfs
HOOKS=(base systemd autodetect microcode kms ignition modconf block keyboard sd-vconsole filesystems)

sudo mkinitcpio -P
```

`kms` includes the early DRM modules and firmware; putting the splash
after it makes the card available as early as the driver permits. A GPU
that must be present before autodetection can also be named in `MODULES`
(for example `MODULES=(amdgpu)`). The classic hook starts a runscript;
when the `systemd` hook is present, it installs the boot and switch-root
units instead.

`mlx.splash=0` or `nosplash` on the kernel command line keeps the splash
away. `plymouth.enable=0` only controls Plymouth and intentionally does
not disable its replacement.

## How it draws

Everything is measured in a sixteenth of the widest 16:9 box the screen
holds, so the picture keeps its proportions at any size. The CPU draws
it into memory, and what changed is copied to the screen.

- The backdrop is drawn once: a dithered radial gradient, bokeh, stars,
  faint constellations.
- Light is added in packed words: red and blue in one multiply, then a
  saturating add across the channels at once.
- The screen is cut into tiles of 32x32 pixels. A frame marks the tiles
  it draws on; the next one restores only those from the backdrop, and
  the screen's buffer takes only the tiles its last frame or this one
  drew on (a frame draws on about a fifth of the screen).
- Once they have come in (1.75 s), the parts that stand still are drawn
  into the backdrop: the dial and the letters' glow.
- What remains per frame is what moves: the rings, their arcs and glints,
  the atoms, dust and text. A frame is recorded first (its sparks, glows,
  lines, the letters, text, the bar), then drawn in bands of 64 rows by
  every processor, the longest bands first. Each band copies its tiles to
  the screen's buffer as soon as it is drawn, while they are in the cache.
- A halo is its colour at a weight in 256ths, so each sprite's glows are
  made once per light and then added two pixels at a time in one 64-bit
  word. Text is laid out once per line, not per frame. The far side of a
  ring has no halo.
- A frame, copied to the screen, takes on four cores: 3840x2160 about
  8 ms, 2560x1440 about 5 ms, 1920x1080 about 4 ms (one core: 22, 12 and
  8 ms); 4K runs at 60 frames a second on four cores.
- It draws 60 frames a second at most, and spends half the time drawing
  at most: a frame never comes sooner than twice the last one's cost. The
  threads helping the first are niced, so a machine busy starting gets
  fewer frames rather than a slower start.
- Each turn of the loop gives back what it allocated (std.turns), so a
  long boot does not grow the splash.

| File | What |
|---|---|
| `paint.mlx` | the surface, packed colour arithmetic, sparks, glows, lines, copies |
| `logo.mlx` | the MLX letters as shaded facets (std.ui.vector) and their glow |
| `state.mlx` | the scene's data, shared by the drawing modules |
| `bands.mlx` | sorting a recorded frame into bands, the threads drawing them, tiles back from the backdrop and out to the screen |
| `captions.mlx` | lines of text laid out once and laid over each frame |
| `backdrop.mlx` | the night, and baking the still parts into it |
| `orbits.mlx` | turning rings, arcs, glints, the code graph's atoms, the dial, dust, stars |
| `status.mlx` | the status in brackets, the message, the progress bar |
| `scene.mlx` | setting up and running a frame |
| `output.mlx` | KMS, fbdev, memory (for `--bench`) or nothing; the terminal's graphics mode |
| `protocol.mlx` | Plymouth's protocol, server and client |
| `daemon.mlx` | the daemon's loop |
| `handover.mlx` | ending the splash from the program taking the screen |
| `tool.mlx` | ignitionctl |
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
  - ignitionctl's commands, quit answered when done, the boot's length
    kept;
- taking over from a stand-in plymouthd;
- the initramfs new-root handoff in a disposable user namespace, including
  persistent state written below the real root;
- classic and systemd mkinitcpio hook installation, dependency copying,
  target links, and both real-root duplicate guards;
- booting before a KMS node exists and probing a card that appears later;
- an emulated KMS card (`tools/fake_drm_session.py`):
  - the mode set and page flips, the frame's pixels;
  - stepping aside for a password and coming back;
  - deactivate and reactivate;
  - quit restoring the console, and quit --retain-splash leaving the
    frame with `CLOSEFB`;
  - the card going away.

## Not yet

- impulse starts it at boot only, not at shutdown.
- No themes: the picture is the MLX one.
