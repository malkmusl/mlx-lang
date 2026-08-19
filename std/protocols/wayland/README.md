# Wayland protocol bootstrap inputs

Canonical `wayland.xml` and selected extension XML files are placed or acquired here by the Stage-0/Stage-1 stdlib bootstrap process. They are source inputs for materializing `std.wayland`; they are not user-project build inputs.

The repository intentionally does not vendor an invented substitute XML file. Bootstrap tooling must preserve upstream protocol contents exactly and record source/version/hash metadata.
