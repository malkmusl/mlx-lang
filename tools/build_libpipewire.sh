#!/usr/bin/env bash
# Builds MLX Capture's client library (projects/desktop/libpipewire): the
# Mlx stand-in for PipeWire's libpipewire-0.3-0, so that the programs
# that take a screen share or a camera over PipeWire (browsers, OBS,
# GStreamer's pipewiresrc) reach mlx-capture's PipeWire socket without
# PipeWire's package.
#
#   tools/build_libpipewire.sh [options]
#     --out DIR        where to put it (default: mlx-out/libpipewire)
#     --deb            also build a Debian package (DIR/mlx-capture-libs_*.deb)
#                      that installs it where the package had its library
#                      and replaces that package (apt install ./FILE.deb)
#     --compiler PATH  the Mlx compiler (default: tools/ensure_compiler.sh)
#
# Builds, into DIR:
#   libpipewire-0.3.so.0   PipeWire's client library (its soname, every
#                          function of libpipewire 1.0.5 and its two data
#                          symbols, so programs linked against it load this)
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"

out=mlx-out/libpipewire
deb=0
compiler=""
while [[ $# -gt 0 ]]; do
    case $1 in
    --out) out=$2; shift ;;
    --deb) deb=1 ;;
    --compiler) compiler=$2; shift ;;
    -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) echo "build_libpipewire.sh: unknown argument $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done
[[ -n "$compiler" ]] || compiler=$(tools/ensure_compiler.sh)
mkdir -p "$out"

"$compiler" --quiet --plugin --soname=libpipewire-0.3.so.0 projects/desktop/libpipewire/libpipewire.mlx -o "$out/libpipewire-0.3.so.0"
echo "built $out/libpipewire-0.3.so.0"
[[ $deb -eq 1 ]] || exit 0

# The package: the library where Debian and Ubuntu keep PipeWire's; it
# provides, and replaces, the package that had it (libpipewire-0.3-0 on
# Debian, libpipewire-0.3-0t64 on Ubuntu 24.04 and later).
command -v dpkg-deb > /dev/null || { echo "build_libpipewire.sh: --deb needs dpkg-deb" >&2; exit 2; }
multiarch=$(dpkg-architecture -qDEB_HOST_MULTIARCH 2> /dev/null || echo x86_64-linux-gnu)
architecture=$(dpkg-architecture -qDEB_HOST_ARCH 2> /dev/null || echo amd64)
version="1.0.5+mlx1"
staged=$(mktemp -d)
trap 'rm -rf -- "$staged"' EXIT
chmod 755 "$staged"
libdir="$staged/usr/lib/$multiarch"
install -d "$libdir" "$staged/usr/share/doc/mlx-capture-libs" "$staged/DEBIAN"
install -m 644 "$out/libpipewire-0.3.so.0" "$libdir/"
cat > "$staged/usr/share/doc/mlx-capture-libs/copyright" <<'TEXT'
MLX Capture's client library, written in Mlx (projects/desktop/libpipewire
in the mlx-lang repository). It implements the interface of PipeWire's
libpipewire, whose behavior it follows.
TEXT
printf 'activate-noawait ldconfig\n' > "$staged/DEBIAN/triggers"
cat > "$staged/DEBIAN/control" <<CONTROL
Package: mlx-capture-libs
Version: $version
Architecture: $architecture
Maintainer: mlx-lang <noreply@mlx-lang.invalid>
Section: libs
Priority: optional
Depends: libc6
Provides: libpipewire-0.3-0 (= $version), libpipewire-0.3-0t64 (= $version)
Conflicts: libpipewire-0.3-0, libpipewire-0.3-0t64
Replaces: libpipewire-0.3-0, libpipewire-0.3-0t64
Description: MLX Capture's PipeWire client library
 libpipewire-0.3.so.0 for the programs that take a screen share or a
 camera over PipeWire (Firefox, Chromium and Electron apps, OBS,
 GStreamer's pipewiresrc), written in Mlx: it speaks PipeWire's protocol
 to the socket the ScreenCast portal hands them, which in the MLX session
 is mlx-capture's. It stands in for libpipewire-0.3-0 (libpipewire-0.3-0t64);
 the PipeWire daemon's own packages, which need that library, go with it.
CONTROL
package="$out/mlx-capture-libs_${version}_$architecture.deb"
dpkg-deb --root-owner-group --build "$staged" "$package" > /dev/null
[[ "$package" == /* ]] || package="./$package"
echo "built $package (replaces libpipewire-0.3-0, libpipewire-0.3-0t64: sudo apt install $package)"
