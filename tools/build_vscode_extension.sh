#!/usr/bin/env bash
# Builds the Mlx language server (tools/mlx-lsp) into the VS Code extension
# (vscode-extension/bin/mlx-lsp) and packs the extension again
# (vscode-extension/mlx-vscode-extension-VERSION.vsix, VERSION from its
# package.json): the previous package with the new server and manifest in
# it, so neither npm nor vsce is needed. Install it with
# `code --install-extension vscode-extension/mlx-vscode-extension-*.vsix`.
#
#   tools/build_vscode_extension.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-}
[[ -n "$compiler" ]] || compiler=$(tools/ensure_compiler.sh)
"$compiler" --quiet tools/mlx-lsp/main.mlx -o vscode-extension/bin/mlx-lsp
echo "built vscode-extension/bin/mlx-lsp"
python3 - <<'PY'
import glob, json, os, re, zipfile
package = json.load(open('vscode-extension/package.json'))
version = package['version']
sources = sorted(glob.glob('vscode-extension/mlx-vscode-extension-*.vsix'))
if not sources:
    raise SystemExit('no earlier .vsix to start from')
source = sources[-1]
target = f'vscode-extension/mlx-vscode-extension-{version}.vsix'
temporary = target + '.new'
with zipfile.ZipFile(source) as old, zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED) as new:
    for item in old.infolist():
        data = old.read(item.filename)
        if item.filename == 'extension/bin/mlx-lsp':
            data = open('vscode-extension/bin/mlx-lsp', 'rb').read()
        elif item.filename == 'extension/package.json':
            data = open('vscode-extension/package.json', 'rb').read()
        elif item.filename == 'extension.vsixmanifest':
            data = re.sub(rb'(<Identity [^>]*Version=")[^"]*', rb'\g<1>' + version.encode(), data)
        info = zipfile.ZipInfo(item.filename, item.date_time)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = item.external_attr
        new.writestr(info, data)
for old_path in sources:
    if old_path != target:
        os.remove(old_path)
os.replace(temporary, target)
print('packed ' + target)
PY
