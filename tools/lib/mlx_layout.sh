# The MLX installation layout, shared by the installers (sourced, not run):
# programs in PREFIX/bin/mlx, data in PREFIX/share, libraries in
# PREFIX/lib, with PREFIX /usr unless --prefix says otherwise. The
# programs keep to a directory of their own beside the distribution's
# /usr/bin; it is not on PATH by itself, so mlx_install_path_files puts it
# there, after the distribution's directories, for every way a PATH comes
# about:
#   login shells                      /etc/profile.d/mlx.sh
#   the systemd user session          /etc/environment.d/50-mlx.conf
#   the system services               /etc/systemd/system.conf.d/50-mlx-path.conf
#   sudo (its secure_path)            /etc/sudoers.d/50-mlx-path
#   su and the console login          ENV_PATH and ENV_SUPATH in /etc/login.defs
#   PAM sessions (ssh, display        PATH in /etc/environment, when it
#   managers, cron)                   sets one
#   fish                              /etc/fish/conf.d/mlx.fish
# The two edited files get BINDIR appended to their PATH values, which is
# what mlx_remove_path_files takes away again. mlx-session, mlx-sh,
# mlx-init, mlx-login and MLXIPC put /usr/bin/mlx on PATH themselves.

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
    /etc/sudoers.d/50-mlx-path
    /etc/fish/conf.d/mlx.fish
)

# Appends (MODE add) or takes away again (MODE remove) :BINDIR in the PATH
# values of FILE: the ENV_PATH and ENV_SUPATH lines of login.defs
# ("ENV_PATH<tab>PATH=a:b") and the PATH line of /etc/environment
# (PATH="a:b"). Other lines stay byte for byte; FILE is written (by
# RUNNER) only when a line changed, keeping its owner and mode.
mlx_edit_path_values() {
    local mode=$1 file=$2 bindir=$3 runner=${4:-}
    local line prefix value rest changed=0 work
    local defs_pattern='^([[:space:]]*ENV_(SU)?PATH[[:space:]]+PATH=)([^[:space:]]*)(.*)$'
    local environment_pattern='^(PATH="?)([^"]*)("?[[:space:]]*)$'
    [[ -f "$file" ]] || return 0
    work=$(mktemp)
    while IFS= read -r line || [[ -n "$line" ]]; do
        prefix=""
        if [[ "$line" =~ $defs_pattern ]]; then
            prefix=${BASH_REMATCH[1]} value=${BASH_REMATCH[3]} rest=${BASH_REMATCH[4]}
        elif [[ "$line" =~ $environment_pattern ]]; then
            prefix=${BASH_REMATCH[1]} value=${BASH_REMATCH[2]} rest=${BASH_REMATCH[3]}
        fi
        if [[ -n "$prefix" ]]; then
            if [[ $mode == add && -n "$value" && ":$value:" != *":$bindir:"* ]]; then
                line="$prefix${value:+$value:}$bindir$rest"
                changed=1
            elif [[ $mode == remove && "$value" == *":$bindir" ]]; then
                line="$prefix${value%":$bindir"}$rest"
                changed=1
            fi
        fi
        printf '%s\n' "$line" >> "$work"
    done < "$file"
    if [[ $changed -eq 1 ]]; then
        $runner cp -- "$work" "$file"
        if [[ $mode == add ]]; then
            echo "changed $file: $bindir appended to its PATH values"
        else
            echo "changed $file: $bindir taken from its PATH values"
        fi
    fi
    rm -f -- "$work"
}

# The secure_path sudo is set to now: the last one /etc/sudoers names,
# else the one Debian's sudoers names (with Ubuntu's /snap/bin).
mlx_sudo_secure_path() {
    local destdir=$1 runner=${2:-} found
    found=$($runner sed -n 's/^[[:space:]]*Defaults[[:space:]][[:space:]]*secure_path[[:space:]]*=[[:space:]]*"\{0,1\}\([^"]*\)"\{0,1\}[[:space:]]*$/\1/p' "$destdir/etc/sudoers" 2> /dev/null | tail -n 1 || true)
    echo "${found:-/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin}"
}

# Installs the PATH additions for BINDIR under DESTDIR; RUNNER (may be
# empty) runs the install commands (sudo for one that is not root).
mlx_install_path_files() {
    local destdir=$1 bindir=$2 runner=${3:-}
    local work secure also=""
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
        also="$also, fish"
    fi
    # sudo replaces PATH by its secure_path: the one it has, with BINDIR.
    # visudo checks the drop-in first (a broken one would lock sudo out).
    # Its name has no dot: sudo skips such files in /etc/sudoers.d.
    if [[ -d "$destdir/etc/sudoers.d" ]]; then
        secure=$(mlx_sudo_secure_path "$destdir" "$runner")
        secure=${secure%":$bindir"}
        printf '# The MLX programs (PREFIX/bin/mlx), after the system'"'"'s own.\nDefaults secure_path="%s:%s"\n' "$secure" "$bindir" > "$work/50-mlx-path"
        if ! command -v visudo > /dev/null 2>&1; then
            echo "note: no visudo to check a sudoers drop-in with; sudo keeps its PATH"
        elif visudo -c -q -f "$work/50-mlx-path" > /dev/null 2>&1; then
            $runner install -m 440 "$work/50-mlx-path" "$destdir/etc/sudoers.d/50-mlx-path"
            also="$also, sudo"
        else
            echo "note: visudo rejected the sudoers drop-in; sudo keeps its PATH"
        fi
    fi
    rm -rf -- "$work"
    mlx_edit_path_values add "$destdir/etc/login.defs" "$bindir" "$runner"
    mlx_edit_path_values add "$destdir/etc/environment" "$bindir" "$runner"
    echo "installed the PATH additions for $bindir (login shells, the user session, the system services$also, su and the console login, PAM sessions); in effect from the next login and boot"
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
    mlx_edit_path_values remove "$destdir/etc/login.defs" "$bindir" "$runner"
    mlx_edit_path_values remove "$destdir/etc/environment" "$bindir" "$runner"
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
