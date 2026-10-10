# Projects

Programs written in Mlx that are used as programs, not as examples.
Each builds with the Mlx compiler alone (`mlx-out/bin/compiler/mlx4
projects/.../main.mlx -o ...`) and is checked by a script under `tools/`.

- [`desktop/`](desktop/): the Mlx desktop, a Wayland session.
  - [`compositor/`](desktop/compositor/README.md): the Wayland compositor
    (nested or on DRM/KMS, CPU or Vulkan composition, hot-reloadable
    shell and renderer); `tools/check_wayland_compositor.sh`,
    `tools/check_compositor_drm.sh`, `tools/check_compositor_modules.sh`.
  - `dock/`, `launcher/`, `topbar/`, `settings/`, `files/`: the desktop's
    clients (the dock, the application launcher, the top bar, the
    settings and the file manager); `tools/check_desktop_clients.sh`.
    The compositor's README describes them.
  - [`terminal/`](desktop/terminal/README.md): the terminal emulator.
  - `shared/`: what the desktop's clients share: the drawing canvas (CPU
    and GPU), the layer-shell and window surface (`panel.mlx`), keyboard
    text, desktop entries and icons, places and the trash, thumbnails,
    the dock's pins, hotkeys and the clock's text.
- [`android/files/`](android/files/README.md): the file manager on
  Android (the same responsive UI); `tools/emulate_android_files.py`.
- [`filemanager-shared/`](filemanager-shared/README.md): the file
  manager's responsive layout and chrome, shared by `desktop/files` and
  `android/files`.
- [`observatory/`](observatory/README.md): MLX Observatory, an IDE around
  a 3D map of the code (`tools/codemap` is its index, shared with
  `tools/mlx-lsp`); `tools/check_observatory.sh`.
- [`coreutils/`](coreutils/README.md): Linux userland programs;
  `projects/coreutils/build.sh` and `test.sh`.
- [`libc/`](libc/README.md): mlxlibc, the C library and dynamic loader in
  Mlx for legacy programs; `tools/check_mlxlibc.sh`.
- [`init/`](init/README.md): mlx-init, PID 1 of MLX/Linux (mounts,
  services, supervision, the control socket, shutdown) and mlx-initctl;
  `tools/check_init.sh`.

What the projects share beyond one project lives in `std` (`std.gpu`,
`std.crash`, `std.timezone`, `std.ui`, `std.png`, `std.truetype`, ...).
`codemap` keeps `std` and the compiler from importing anything under
`projects/`.
