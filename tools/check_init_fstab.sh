#!/usr/bin/env bash
# Focused check for mlx-init's complete fstab fields and fsck pass ordering.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

"$compiler" --quiet tools/init-fstab-test/main.mlx -o "$work/fstab-test"
cat > "$work/fsck" <<'CHECKER'
#!/bin/sh
printf '%s\n' "$*" >> "$(dirname "$0")/fsck.log"
CHECKER
chmod 755 "$work/fsck"
cat > "$work/fstab" <<'FSTAB'
LABEL=root\040source /root ext4 defaults 1 1
ignored-source /ignored ext4 noauto 0 1
swap-source none swap sw 0 1
PARTUUID=late-source /late xfs defaults 0 2
unchecked-source /unchecked ext4 defaults 0 0
FSTAB

"$work/fstab-test" "$work/fstab" "$work/fsck"
mapfile -t calls < "$work/fsck.log"
[[ ${#calls[@]} -eq 2 ]] || { printf 'unexpected fsck calls:\n%s\n' "${calls[*]-}" >&2; exit 1; }
[[ "${calls[0]}" == '-a -t ext4 /dev/disk/by-label/root source' ]] || { echo "wrong pass-1 call: ${calls[0]}" >&2; exit 1; }
[[ "${calls[1]}" == '-a -t xfs /dev/disk/by-partuuid/late-source' ]] || { echo "wrong pass-2 call: ${calls[1]}" >&2; exit 1; }

echo 'ok   fstab reads six fields, resolves tags, decodes escapes, skips entries, and runs fsck in pass order'

cat > "$work/fsck-fails" <<'CHECKER'
#!/bin/sh
printf '%s\n' "$*" >> "$(dirname "$0")/fsck-fails.log"
exit 4
CHECKER
chmod 755 "$work/fsck-fails"
if "$work/fstab-test" "$work/fstab" "$work/fsck-fails"; then
    echo 'a failed file-system check was accepted' >&2
    exit 1
fi
mapfile -t failed_calls < "$work/fsck-fails.log"
[[ ${#failed_calls[@]} -eq 1 ]] || { printf 'checks continued after failure:\n%s\n' "${failed_calls[*]-}" >&2; exit 1; }
[[ "${failed_calls[0]}" == '-a -t ext4 /dev/disk/by-label/root source' ]] || { echo "wrong failing check: ${failed_calls[0]}" >&2; exit 1; }

echo 'ok   an uncorrected fsck error stops later checks and fails the boot gate'
