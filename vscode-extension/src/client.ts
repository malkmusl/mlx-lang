import * as vscode from 'vscode';
import * as path from 'path';

export function getLspBinaryPath(context: vscode.ExtensionContext): string {
  const binDir = path.join(context.extensionPath, 'bin');
  const exe = process.platform === 'win32' ? 'zin-lsp.exe' : 'zin-lsp';
  return path.join(binDir, exe);
}
