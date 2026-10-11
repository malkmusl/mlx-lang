# The MLX installation layout, shared by the installers (sourced, not run):
# programs in PREFIX/bin/mlx, data in PREFIX/share, libraries in
# PREFIX/lib, with PREFIX /usr unless --prefix says otherwise. The
# programs keep to a directory of their own beside the distribution's
# /usr/bin; it is not on PATH by itself, so mlx_install_path_files adds it
# after the distribution's directories for login shells
# (/etc/profile.d/mlx.sh), the systemd user session and what it starts
# (/etc/environment.d/50-mlx.conf), the system services
# (/etc/systemd/system.conf.d/50-mlx-path.conf) and fish
# (/etc/fish/conf.d/mlx.fish, when fish is there). mlx-session, mlx-sh,
# mlx-init, mlx-login and MLXIPC know /usr/bin/mlx without them.

mlx_default_prefix=/usr

# PREFIX/bin/mlx
mlx_bindir() {
    local prefix=${1%/}
    echo "$prefix/bin/mlx"
}

# Every program an MLX installer puts in its bin directory, now or in the
# layouts before PREFIX/bin/mlx (/usr/local/bin, /usr/bin).
mlx_all_programs=(
    mlx-compositor mlx-terminal mlx-sh mlx-console mlx-session mlx-dock
    mlx-topbar mlx-launcher mlx-settings mlx-files mlx-observatory
    mlx-codemap mlx-profile mlx-capture mlx-ipcd mlx-ipc mlx-audiod
    mlx-audio mlx-permissions mlx-notifyd mlx-notify
    mlx-login mlx-greeter mlx-lock
    mlx-netd mlx-net
)

# The files mlx_install_path_files writes, relative to DESTDIR.
mlx_path_files=(
    /etc/profile.d/mlx.sh
    /etc/environment.d/50-mlx.conf
    /etc/systemd/system.conf.d/50-mlx-path.conf
    /etc/fish/conf.d/mlx.fish
)

# Installs the PATH additions for BINDIR under DESTDIR; RUNNER (may be
# empty) runs the install commands (sudo for one that is not root).
mlx_install_path_files() {
    local destdir=$1 bindir=$2 runner=${3:-}
    local work fish=""
    work=$(mktemp -d)
    cat > "$work/mlx.sh" <<PROFILE
# The MLX programs (PREFIX/bin/mlx), after the distribution's own.
case ":\$PATH:" in
    *":$bindir:"*) ;;
    *) PATH="\$PATH:$bindir"; export PATH ;;
esac
PROFILE
    printf '# The MLX programs, for the systemd user session.\nPATH=${PATH}:%s\n' "$bindir" > "$work/50-mlx.conf"
    # The system manager's own PATH for services (systemd's default for a
    # merged /usr, then /sbin and /bin for one that is not) plus BINDIR.
    printf '# The MLX programs, for the system services.\n[Manager]\nDefaultEnvironment="PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:%s"\n' "$bindir" > "$work/50-mlx-path.conf"
    printf '# The MLX programs (PREFIX/bin/mlx), after the distribution'"'"'s own.\ncontains -- %s $PATH; or set -gx PATH $PATH %s\n' "$bindir" "$bindir" > "$work/mlx.fish"
    $runner install -d "$destdir/etc/profile.d" "$destdir/etc/environment.d" "$destdir/etc/systemd/system.conf.d"
    $runner install -m 644 "$work/mlx.sh" "$destdir/etc/profile.d/mlx.sh"
    $runner install -m 644 "$work/50-mlx.conf" "$destdir/etc/environment.d/50-mlx.conf"
    $runner install -m 644 "$work/50-mlx-path.conf" "$destdir/etc/systemd/system.conf.d/50-mlx-path.conf"
    if [[ -d "$destdir/etc/fish" ]]; then
        $runner install -d "$destdir/etc/fish/conf.d"
        $runner install -m 644 "$work/mlx.fish" "$destdir/etc/fish/conf.d/mlx.fish"
        fish=", fish"
    fi
    rm -rf -- "$work"
    echo "installed the PATH additions for $bindir (login shells, the user session, the system services$fish); in effect from the next login and boot"
}

# Removes the PATH additions and the empty BINDIR once no program is left
# in BINDIR (an uninstall of one installer leaves the others' programs).
mlx_remove_path_files() {
    local destdir=$1 bindir=$2 runner=${3:-} program path
    for program in "${mlx_all_programs[@]}"; do
        [[ -e "$destdir$bindir/$program" || -L "$destdir$bindir/$program" ]] && return 0
    done
    for path in "${mlx_path_files[@]}"; do
        if [[ -e "$destdir$path" ]]; then
            $runner rm -f -- "$destdir$path"
            echo "removed $destdir$path"
        fi
    done
    if [[ -d "$destdir$bindir" ]]; then
        $runner rmdir --ignore-fail-on-non-empty -- "$destdir$bindir"
    fi
    return 0
}

# Says so when copies from the layouts before PREFIX/bin/mlx are still
# there (tools/remove_old_mlx_layout.sh lists and removes them).
mlx_note_old_layout() {
    local destdir=$1 bindir=$2 dir program found=0
    for dir in /usr/local/bin /usr/bin; do
        [[ "$dir" == "$bindir" ]] && continue
        for program in "${mlx_all_programs[@]}"; do
            if [[ -e "$destdir$dir/$program" ]]; then found=1; fi
        done
    done
    if [[ $found -eq 1 ]]; then
        echo "note: copies from before $bindir are still in /usr/local/bin or /usr/bin;"
        echo "      tools/remove_old_mlx_layout.sh shows them, with --remove it removes them"
    fi
    return 0
}
