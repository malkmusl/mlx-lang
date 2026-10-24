# Building for the installers (sourced, not run; REPO_ROOT is the
# checkout): without a compiler named, the programs come from
# `./mlx build` (into mlx-out/bin, only what is not current), with one
# named they are compiled with it directly (the installers' checks use a
# stand-in compiler that way).

# Runs `./mlx build ARGS...` as the owner of the checkout: an installer
# started with sudo leaves nothing owned by root in mlx-out.
mlx_build() {
    local owner
    owner=$(stat -c %U "$repo_root")
    if [[ $(id -u) -eq 0 && "$owner" != root ]] && command -v sudo > /dev/null; then
        sudo -u "$owner" "$repo_root/mlx" build "$@"
    else
        "$repo_root/mlx" build "$@"
    fi
}
