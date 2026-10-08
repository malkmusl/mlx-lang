#!/usr/bin/env bash
# Builds MLX Audio's client libraries (projects/desktop/libpulse): Mlx
# stand-ins for PulseAudio's libpulse0 and libpulse-mainloop-glib0 and for
# alsa-plugins' pulse plugin (libasound2-plugins), so that neither package
# is needed for programs to reach MLX Audio.
#
#   tools/build_libpulse.sh [options]
#     --out DIR        where to put them (default: mlx-out/libpulse)
#     --deb            also build a Debian package (DIR/mlx-audio-libs_*.deb)
#                      that installs them where the packages had theirs and
#                      replaces those packages (apt install ./FILE.deb)
#     --compiler PATH  the Mlx compiler (default: tools/ensure_compiler.sh)
#
# Builds, into DIR:
#   libpulse.so.0                 PulseAudio's client library (soname and
#   libpulse-simple.so.0          symbol version PULSE_0 as PulseAudio's, so
#   libpulse-mainloop-glib.so.0   programs linked against those load them)
#   libasound_module_pcm_pulse.so ALSA's pulse PCM and control plugin (with
#   libasound_module_ctl_pulse.so its own libpulse built in; the same file)
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"

out=mlx-out/libpulse
deb=0
compiler=""
while [[ $# -gt 0 ]]; do
    case $1 in
    --out) out=$2; shift ;;
    --deb) deb=1 ;;
    --compiler) compiler=$2; shift ;;
    -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) echo "build_libpulse.sh: unknown argument $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done
[[ -n "$compiler" ]] || compiler=$(tools/ensure_compiler.sh)
mkdir -p "$out"

source=projects/desktop/libpulse
"$compiler" --quiet --plugin --soname=libpulse.so.0 --symbol-version=PULSE_0 "$source/libpulse.mlx" -o "$out/libpulse.so.0"
"$compiler" --quiet --plugin --library libpulse.so.0 --soname=libpulse-simple.so.0 --symbol-version=PULSE_0 "$source/simple.mlx" -o "$out/libpulse-simple.so.0"
"$compiler" --quiet --plugin --library libglib-2.0.so.0 --soname=libpulse-mainloop-glib.so.0 --symbol-version=PULSE_0 "$source/glib.mlx" -o "$out/libpulse-mainloop-glib.so.0"
"$compiler" --quiet --plugin --library libasound.so.2 "$source/alsa.mlx" -o "$out/libasound_module_pcm_pulse.so"
cp "$out/libasound_module_pcm_pulse.so" "$out/libasound_module_ctl_pulse.so"
echo "built $out: libpulse.so.0, libpulse-simple.so.0, libpulse-mainloop-glib.so.0, libasound_module_pcm_pulse.so, libasound_module_ctl_pulse.so"
[[ $deb -eq 1 ]] || exit 0

# The package: the libraries where Debian and Ubuntu keep PulseAudio's,
# the plugin in alsa-lib's directory, ALSA's pulse device; it provides,
# and replaces, the packages that had them.
command -v dpkg-deb > /dev/null || { echo "build_libpulse.sh: --deb needs dpkg-deb" >&2; exit 2; }
multiarch=$(dpkg-architecture -qDEB_HOST_MULTIARCH 2> /dev/null || echo x86_64-linux-gnu)
architecture=$(dpkg-architecture -qDEB_HOST_ARCH 2> /dev/null || echo amd64)
version="1:16.1+mlx1"
staged=$(mktemp -d)
trap 'rm -rf -- "$staged"' EXIT
chmod 755 "$staged"
libdir="$staged/usr/lib/$multiarch"
install -d "$libdir/alsa-lib" "$staged/usr/share/alsa/alsa.conf.d" "$staged/usr/share/doc/mlx-audio-libs" "$staged/DEBIAN"
install -m 644 "$out/libpulse.so.0" "$out/libpulse-simple.so.0" "$out/libpulse-mainloop-glib.so.0" "$libdir/"
install -m 644 "$out/libasound_module_pcm_pulse.so" "$out/libasound_module_ctl_pulse.so" "$libdir/alsa-lib/"
cat > "$staged/usr/share/alsa/alsa.conf.d/50-pulseaudio.conf" <<'CONF'
# ALSA's pulse device: MLX Audio's plugin (mlx-audio-libs).
pcm.pulse {
    type pulse
    hint {
        show on
        description "MLX Audio"
    }
}
ctl.pulse {
    type pulse
}
CONF
cat > "$staged/usr/share/doc/mlx-audio-libs/copyright" <<'TEXT'
MLX Audio's client libraries, written in Mlx (projects/desktop/libpulse in
the mlx-lang repository). They implement the interfaces of PulseAudio's
libpulse and of alsa-plugins' pulse plugin, whose behavior they follow.
TEXT
printf 'activate-noawait ldconfig\n' > "$staged/DEBIAN/triggers"
cat > "$staged/DEBIAN/control" <<CONTROL
Package: mlx-audio-libs
Version: $version
Architecture: $architecture
Maintainer: mlx-lang <noreply@mlx-lang.invalid>
Section: libs
Priority: optional
Depends: libc6, libasound2t64 | libasound2, libglib2.0-0t64 | libglib2.0-0
Provides: libpulse0 (= $version), libpulse-mainloop-glib0 (= $version), libasound2-plugins (= 1.2.7.1+mlx1)
Conflicts: libpulse0, libpulse-mainloop-glib0, libasound2-plugins
Replaces: libpulse0, libpulse-mainloop-glib0, libasound2-plugins
Description: MLX Audio's PulseAudio client libraries and ALSA plugin
 libpulse.so.0, libpulse-simple.so.0 and libpulse-mainloop-glib.so.0 for
 the programs that speak PulseAudio (Firefox, Chromium and Electron apps,
 SDL games, GStreamer, pavucontrol), and ALSA's pulse plugin, written in
 Mlx: they reach MLX Audio's PulseAudio socket. They stand in for
 libpulse0, libpulse-mainloop-glib0 and libasound2-plugins.
CONTROL
package="$out/mlx-audio-libs_${version#*:}_$architecture.deb"
dpkg-deb --root-owner-group --build "$staged" "$package" > /dev/null
[[ "$package" == /* ]] || package="./$package"
echo "built $package (replaces libpulse0, libpulse-mainloop-glib0, libasound2-plugins: sudo apt install $package)"
