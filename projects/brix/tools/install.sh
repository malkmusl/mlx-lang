# install [VOLUME]: brixOS onto a volume of a disk (/volumes/disk0 unless
# named): /system (the programs and settings), /users and /temporary
# copied there, then committed. From the next boot on the kernel takes a
# volume with system/programs/mlx-sh as the system: its /system, /users
# and /temporary are the disk's, and the boot archive's programs stay
# reachable at /volumes/boot (from where a system booted from the disk
# installs again).
volume=${1:-/volumes/disk0}
if [ ! -d "$volume" ]; then
    echo "install: no volume at $volume" >&2
    exit 1
fi
source=/system
if [ -d /volumes/boot ]; then source=/volumes/boot; fi
if [ -e "$volume/system" ]; then
    echo "install: $volume holds a system already (remove its system directory first)" >&2
    exit 1
fi
cp -r "$source" "$volume/system" || exit 1
cp -r /users "$volume/users" || exit 1
mkdir -p "$volume/temporary" || exit 1
chmod 1777 "$volume/temporary"
sync
echo "installed on $volume: it is the system from the next boot on"
