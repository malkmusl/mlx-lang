#!/usr/bin/env bash
# Builds and installs the MLX display-manager pieces without changing the
# active display manager unless --enable is present.
#
# Installs:
#   PREFIX/bin/mlx-login, mlx-greeter and mlx-lock
#   /etc/systemd/system/mlx-login.service
#   /etc/mlx/init/services/mlx-ipcd.service, mlx-netd.service and
#                                  mlx-login.service
#   PREFIX/share/doc/mlx/login/README.md
#
# Usage: tools/install_login.sh [options] [compiler]
#   --prefix DIR       programs go to DIR/bin (default /usr/local)
#   --destdir DIR      stage every path below DIR; never calls systemctl
#   --compiler PATH    Mlx compiler (a final positional path also works)
#   --enable           make mlx-login the display-manager.service and mask
#                      the display manager that alias previously selected
#   --disable          undo --enable from saved state; installed files stay
#
# Without --destdir the installer must run as root. Enable and disable use
# only explicit links, so the same reversible behavior is testable in a
# staged tree.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"

prefix=/usr/local
destdir=""
compiler=""
enable=false
disable=false

die() {
    echo "install_login.sh: $*" >&2
    exit 2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)
            [[ $# -ge 2 && -n "$2" ]] || die "--prefix needs a directory"
            prefix=$2
            shift 2
            ;;
        --destdir)
            [[ $# -ge 2 && -n "$2" ]] || die "--destdir needs a directory"
            destdir=$2
            shift 2
            ;;
        --compiler)
            [[ $# -ge 2 && -n "$2" ]] || die "--compiler needs a path"
            compiler=$2
            shift 2
            ;;
        --enable)
            enable=true
            shift
            ;;
        --disable)
            disable=true
            shift
            ;;
        -h|--help)
            sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'
            exit 0
            ;;
        -*) die "unknown option $1 (see --help)" ;;
        *)
            [[ -z "$compiler" ]] || die "more than one compiler was given"
            compiler=$1
            shift
            ;;
    esac
done

$enable && $disable && die "--enable and --disable are mutually exclusive"
[[ "$prefix" == /* ]] || die "--prefix must be an absolute path"
[[ -z "$destdir" || "$destdir" == /* ]] || die "--destdir must be an absolute path"
[[ "$prefix" == / ]] || prefix=${prefix%/}
if [[ "$destdir" == / ]]; then
    destdir=""
elif [[ -n "$destdir" ]]; then
    destdir=${destdir%/}
fi
if [[ -z "$destdir" && $(id -u) -ne 0 ]]; then
    die "run it as root (or stage it with --destdir DIR)"
fi

bindir="${prefix%/}/bin"
docdir="${prefix%/}/share/doc/mlx/login"
system_dir="$destdir/etc/systemd/system"
wants_path="$system_dir/graphical.target.wants/mlx-login.service"
display_path="$system_dir/display-manager.service"
state_dir="$destdir/var/lib/mlx/login-installer"

remove_path() {
    local path=$1
    if [[ -d "$path" && ! -L "$path" ]]; then
        die "refusing to replace directory $path"
    fi
    if [[ -e "$path" || -L "$path" ]]; then rm -f -- "$path"; fi
}

link_path() {
    local target=$1 path=$2
    remove_path "$path"
    ln -s -- "$target" "$path"
}

# Records an absent path, a symlink verbatim, or a regular file. This is
# deliberately data rather than sourced shell code: link targets never
# become commands during restoration.
capture_path() {
    local label=$1 path=$2 kind
    rm -f -- "$state_dir/$label.kind" "$state_dir/$label.target" "$state_dir/$label.file"
    if [[ -L "$path" ]]; then
        kind=symlink
        readlink -- "$path" > "$state_dir/$label.target"
    elif [[ -f "$path" ]]; then
        kind=file
        cp -a -- "$path" "$state_dir/$label.file"
    elif [[ -e "$path" ]]; then
        die "cannot preserve non-file path $path"
    else
        kind=absent
    fi
    printf '%s\n' "$kind" > "$state_dir/$label.kind"
}

restore_path() {
    local label=$1 path=$2 kind target
    [[ -f "$state_dir/$label.kind" ]] || return 0
    kind=$(< "$state_dir/$label.kind")
    remove_path "$path"
    case "$kind" in
        absent) ;;
        symlink)
            target=$(< "$state_dir/$label.target")
            ln -s -- "$target" "$path"
            ;;
        file) cp -a -- "$state_dir/$label.file" "$path" ;;
        *) die "invalid saved state for $path" ;;
    esac
}

valid_unit_name() {
    [[ "$1" == *.service && "$1" != display-manager.service && "$1" != mlx-login.service && "$1" != *[!A-Za-z0-9_.@-]* ]]
}

enable_login() {
    local previous="" target=""
    install -d "$system_dir/graphical.target.wants"
    install -d -m 700 "$state_dir"
    if [[ ! -f "$state_dir/enabled" ]]; then
        capture_path display-manager "$display_path"
        capture_path graphical-wants "$wants_path"
        if [[ -L "$display_path" ]]; then
            target=$(readlink -- "$display_path")
            previous=${target##*/}
            if ! valid_unit_name "$previous"; then previous=""; fi
        fi
        printf '%s\n' "$previous" > "$state_dir/previous-name"
        if [[ -n "$previous" ]]; then
            capture_path previous-unit "$system_dir/$previous"
        fi
        printf 'enabled\n' > "$state_dir/enabled"
    fi
    previous=$(< "$state_dir/previous-name")
    if [[ -n "$previous" ]]; then
        valid_unit_name "$previous" || die "invalid previous display-manager name in $state_dir"
        link_path /dev/null "$system_dir/$previous"
    fi
    link_path /etc/systemd/system/mlx-login.service "$display_path"
    link_path /etc/systemd/system/mlx-login.service "$wants_path"
    echo "enabled mlx-login.service as display-manager.service"
    if [[ -n "$previous" ]]; then echo "masked the previous display manager: $previous"; fi
}

discard_state() {
    rm -f -- "$state_dir/enabled" "$state_dir/previous-name" \
        "$state_dir/display-manager.kind" "$state_dir/display-manager.target" "$state_dir/display-manager.file" \
        "$state_dir/graphical-wants.kind" "$state_dir/graphical-wants.target" "$state_dir/graphical-wants.file" \
        "$state_dir/previous-unit.kind" "$state_dir/previous-unit.target" "$state_dir/previous-unit.file"
    rmdir -- "$state_dir" 2> /dev/null || true
}

disable_login() {
    local previous=""
    if [[ -f "$state_dir/enabled" ]]; then
        previous=$(< "$state_dir/previous-name")
        if [[ -n "$previous" ]]; then
            valid_unit_name "$previous" || die "invalid previous display-manager name in $state_dir"
            restore_path previous-unit "$system_dir/$previous"
        fi
        restore_path graphical-wants "$wants_path"
        restore_path display-manager "$display_path"
        discard_state
        echo "disabled mlx-login.service and restored the previous display manager state"
    else
        if [[ "$(readlink -- "$wants_path" 2> /dev/null || true)" == /etc/systemd/system/mlx-login.service ]]; then
            remove_path "$wants_path"
        fi
        if [[ "$(readlink -- "$display_path" 2> /dev/null || true)" == /etc/systemd/system/mlx-login.service ]]; then
            remove_path "$display_path"
        fi
        echo "mlx-login.service was already disabled; there was no saved display manager state"
    fi
    [[ -z "$destdir" ]] && echo "run: systemctl daemon-reload"
    return 0
}

if $disable; then
    disable_login
    exit 0
fi

compiler=${compiler:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
build=$(mktemp -d)
cleanup() {
    local status=$?
    trap - EXIT
    rm -rf -- "$build"
    exit "$status"
}
trap cleanup EXIT

"$compiler" --quiet projects/desktop/login/main.mlx -o "$build/mlx-login"
"$compiler" --quiet projects/desktop/greeter/main.mlx -o "$build/mlx-greeter"
"$compiler" --quiet projects/desktop/lock/main.mlx -o "$build/mlx-lock"
echo "built mlx-login, mlx-greeter and mlx-lock"

install -d "$destdir$bindir" "$system_dir" "$destdir/etc/mlx/init/services" "$destdir$docdir"
install -m 755 "$build/mlx-login" "$build/mlx-greeter" "$build/mlx-lock" "$destdir$bindir/"
sed -e "s|@BINDIR@|$bindir|g" -e "s|@DOCDIR@|$docdir|g" \
    projects/desktop/login/examples/mlx-login.service > "$build/mlx-login.service"
sed "s|@BINDIR@|$bindir|g" projects/desktop/ipc/examples/mlx-ipcd.init > "$build/mlx-ipcd.init"
sed "s|@BINDIR@|$bindir|g" projects/network/examples/mlx-netd.init > "$build/mlx-netd.init"
sed "s|@BINDIR@|$bindir|g" projects/desktop/login/examples/mlx-login.init > "$build/mlx-login.init"
install -m 644 "$build/mlx-login.service" "$system_dir/mlx-login.service"
install -m 644 "$build/mlx-ipcd.init" "$destdir/etc/mlx/init/services/mlx-ipcd.service"
install -m 644 "$build/mlx-netd.init" "$destdir/etc/mlx/init/services/mlx-netd.service"
install -m 644 "$build/mlx-login.init" "$destdir/etc/mlx/init/services/mlx-login.service"
install -m 644 projects/desktop/login/README.md "$destdir$docdir/README.md"
echo "installed mlx-login, mlx-greeter, mlx-lock, the systemd unit and the mlx-init service set into ${destdir:-/}"

if $enable; then
    enable_login
    [[ -z "$destdir" ]] && echo "run: systemctl daemon-reload && systemctl start mlx-login.service"
else
    echo "to switch display managers: tools/install_login.sh --enable"
fi
exit 0
