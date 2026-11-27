# Settings (control)

The settings program of the Mlx desktop: a window with the categories on
the left (General, Network, Display, Sound, Apps, Window management, Top
bar, Appearance, Language) and their settings beside them. What it
changes, it changes while the desktop runs: the compositor reads
`~/.config/mlx/compositor.conf` again within half a second, the top bar
its own file within a second, the apps follow `appearance.conf`
([`main.mlx`](main.mlx) says each page in its head).

    control [general|network|display|sound|apps|windows|topbar|appearance|language]

opens on that page (the top bar's buttons use it).

## Display

The Display page shows the monitor the compositor drives, its mode, the
modes to choose from and night light. What the monitor is and what it
offers comes from the compositor, which writes
`$XDG_RUNTIME_DIR/<WAYLAND_DISPLAY>.display` whenever the display changes
([`display.mlx`](display.mlx);
[`../compositor/display.mlx`](../compositor/display.mlx)). The page reads
it again every second while it is shown.

On brixOS a mode is set at once, and kept only when you say so:

1. Settings, Display: "Display" names who sets the modes (QEMU's adapter,
   a Radeon's HDMI or DVI output, or the firmware's picture, which no
   driver changes), then the resolution and the rate that run. Under
   "Resolution and refresh rate" every mode the display can run, the
   largest first: "1920 × 1080, 60 Hz", the monitor's own marked "(the
   monitor's own)", the one that runs chosen. Under "Not available" the
   modes the monitor names that cannot be set, each with the kernel's
   reason ("more than this output carries", "another rate of the size
   that runs, which this driver does not set", ...).
2. A click on a mode asks the compositor for it (`org.mlx.Display.Ask`
   on the channel the file names as `ask`:
   [`../compositor/mode_change.mlx`](../compositor/mode_change.mlx)); the
   compositor asks the kernel, and the screen takes the mode at once.
   The page says what came of it under the list ("Set: keep it or go
   back on the screen", or "Not set: " and why).
3. On the new picture the compositor asks "Keep this display mode?" with
   the seconds left. Enter or Keep keeps it, also for the next boots;
   Escape or Go back brings the mode before back. Without an answer the
   mode before comes back after fifteen seconds by itself.

The page writes nothing for this: the kernel remembers the mode kept
(/system/settings/display-modes, for that monitor on that adapter), and
N held while the machine starts forgets it again.

Elsewhere (Linux) the page writes the mode chosen into compositor.conf
(`display-mode = WxH@HZ`, or `auto`), and the compositor takes it at its
next start; "Automatic" is the monitor's native mode at its fastest
rate.

Night light (warmer colours through the monitor's gamma table) applies
at once where the compositor drives the monitor through DRM; on brixOS
the display has no gamma table, and the page says so.

When the page is shown it says on the program's errors what it shows,
once each time that changes, and what a mode asked for came to:

    control: display: 1024x768 at 60 Hz runs (QEMU's adapter); 20 modes to choose, 4 not available; set at once
    control: display: asked for 1920x1080: Set: keep it or go back on the screen.

tests/492_display_page_runtime.mlx holds the page's reading of the file
and the way it asks; tools/check_brix_modes_desktop.sh opens the page
with the pointer on brixOS and changes the mode from it.
