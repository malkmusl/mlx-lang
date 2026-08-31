import * as vscode from 'vscode';

export function showAstPanel(context: vscode.ExtensionContext, astJson: string) {
  const panel = vscode.window.createWebviewPanel(
    'zinAst',
    'Zin AST Viewer',
    vscode.ViewColumn.One,
    { enableScripts: false }
  );

  const escaped = astJson.replace(/</g, '&lt;').replace(/>/g, '&gt;');
  panel.webview.html = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Zin AST</title>
<style>
  body { font-family: var(--vscode-editor-font-family, monospace); padding: 1rem; }
  pre { white-space: pre-wrap; word-break: break-all; }
</style>
</head>
<body>
<h2>Zin AST (JSON)</h2>
<pre>${escaped}</pre>
</body>
</html>`;
}
