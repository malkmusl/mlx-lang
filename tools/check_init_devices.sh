#!/usr/bin/env bash
# Focused mlx-init device check. All device nodes and sysfs records are
# synthetic; the host's /dev and /sys are never modified.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
cleanup() { rm -rf -- "$work"; }
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

"$compiler" --quiet tools/init-devices-test/main.mlx -o "$work/init-devices-test"
echo "ok   mlx-init device helper builds"

# The fixture's groups: the caller's, unless that is root's (gid 0, which
# the device module reads as "no such group", as it would be for video,
# input and audio); root may hand devices to any group.
gid=$(id -g)
[[ "$gid" == 0 ]] && gid=1000
cat > "$work/group" <<GROUPS
video:x:$gid:
input:x:$gid:
audio:x:$gid:
GROUPS
mkdir -p "$work/sys/class/drm/card0" "$work/sys/class/sound/controlC0" \
    "$work/dev/dri" "$work/dev/input" "$work/dev/snd"
printf 'DEVNAME=dri/card0\nSUBSYSTEM=drm\n' > "$work/sys/class/drm/card0/uevent"
printf 'DEVNAME=snd/controlC0\nSUBSYSTEM=sound\n' > "$work/sys/class/sound/controlC0/uevent"
: > "$work/dev/dri/card0"
: > "$work/dev/input/event0"
: > "$work/dev/snd/controlC0"
chmod 0600 "$work/dev/dri/card0" "$work/dev/input/event0" "$work/dev/snd/controlC0"
"$work/init-devices-test" coldplug "$work/sys" "$work/dev" "$work/group"
[[ $(stat -c %a "$work/dev/dri/card0") == 660 ]] || fail "coldplug did not restrict the DRM device to 0660"
[[ $(stat -c %g "$work/dev/dri/card0") == "$gid" ]] || fail "coldplug did not assign the video group"
[[ $(stat -c %a "$work/dev/snd/controlC0") == 660 ]] || fail "coldplug did not restrict the sound device to 0660"
[[ $(stat -c %g "$work/dev/snd/controlC0") == "$gid" ]] || fail "coldplug did not assign the audio group"
echo "ok   coldplug walks sysfs and assigns DRM and sound permissions"

printf 'add@/devices/input0\0ACTION=add\0DEVNAME=input/event0\0SUBSYSTEM=input\0' > "$work/add.event"
"$work/init-devices-test" event "$work/dev" "$work/group" "$work/add.event"
[[ $(stat -c %a "$work/dev/input/event0") == 660 ]] || fail "the input uevent did not set mode 0660"
[[ $(stat -c %g "$work/dev/input/event0") == "$gid" ]] || fail "the input uevent did not assign the input group"
echo "ok   a hotplug uevent assigns input permissions"

: > "$work/escape"
chmod 0600 "$work/escape"
printf 'ACTION=add\0DEVNAME=input/../../escape\0' > "$work/traversal.event"
"$work/init-devices-test" ignored "$work/dev" "$work/group" "$work/traversal.event"
[[ $(stat -c %a "$work/escape") == 600 ]] || fail "an unsafe DEVNAME escaped the device root"
chmod 0600 "$work/dev/input/event0"
printf 'ACTION=remove\0DEVNAME=input/event0\0' > "$work/remove.event"
"$work/init-devices-test" ignored "$work/dev" "$work/group" "$work/remove.event"
[[ $(stat -c %a "$work/dev/input/event0") == 600 ]] || fail "a remove event changed a possibly reused path"
echo "ok   unsafe and remove uevents are ignored"

if "$work/init-devices-test" socket; then
    echo "ok   the kernel provides the kobject uevent Netlink socket"
else
    echo "skip kobject uevent Netlink socket is unavailable in this environment"
fi
