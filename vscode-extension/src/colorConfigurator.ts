import * as vscode from 'vscode';

export function showColorConfigurator(context: vscode.ExtensionContext) {
  const panel = vscode.window.createWebviewPanel(
    'mlxColorConfigurator',
    'Mlx Theme Configurator',
    vscode.ViewColumn.One,
    {
      enableScripts: true,
      retainContextWhenHidden: true
    }
  );

  const initialColors = {
    keywords: '#FF007F',
    operators: '#FFD700',
    types: '#00FFFF',
    storage: '#FF8C00',
    functions: '#39FF14',
    builtins: '#B500FF',
    parameters: '#FFB6C1',
    numbers: '#FFEA00',
    constants: '#FF3333',
    strings: '#00FF7F',
    comments: '#888888'
  };

  // Try to load existing customizations if any
  const config = vscode.workspace.getConfiguration('editor');
  const tokenColors = config.get<any>('tokenColorCustomizations');
  if (tokenColors && tokenColors['[Mlx Vibrant Dark]'] && tokenColors['[Mlx Vibrant Dark]'].textMateRules) {
    const rules = tokenColors['[Mlx Vibrant Dark]'].textMateRules;
    for (const rule of rules) {
      if (!rule.scope) continue;
      const scopes = Array.isArray(rule.scope) ? rule.scope : [rule.scope];
      if (scopes.includes('keyword.control.mlx')) initialColors.keywords = rule.settings.foreground;
      else if (scopes.includes('keyword.operator.mlx')) initialColors.operators = rule.settings.foreground;
      else if (scopes.includes('entity.name.type.mlx')) initialColors.types = rule.settings.foreground;
      else if (scopes.includes('storage.type.mlx')) initialColors.storage = rule.settings.foreground;
      else if (scopes.includes('entity.name.function.mlx')) initialColors.functions = rule.settings.foreground;
      else if (scopes.includes('support.function.builtin.mlx')) initialColors.builtins = rule.settings.foreground;
      else if (scopes.includes('variable.parameter.mlx')) initialColors.parameters = rule.settings.foreground;
      else if (scopes.includes('constant.numeric.mlx')) initialColors.numbers = rule.settings.foreground;
      else if (scopes.includes('constant.language.mlx')) initialColors.constants = rule.settings.foreground;
      else if (scopes.includes('string.quoted.double.mlx')) initialColors.strings = rule.settings.foreground;
      else if (scopes.includes('comment.line.double-slash.mlx')) initialColors.comments = rule.settings.foreground;
    }
  }

  panel.webview.html = getWebviewContent(initialColors);

  panel.webview.onDidReceiveMessage(async (message) => {
    if (message.command === 'saveColors') {
      const colors = message.colors;
      const newRules = [
        {
          scope: ["keyword.control.mlx"],
          settings: { foreground: colors.keywords, fontStyle: "bold" }
        },
        {
          scope: ["keyword.operator.mlx"],
          settings: { foreground: colors.operators }
        },
        {
          scope: ["entity.name.type.mlx", "support.type.mlx"],
          settings: { foreground: colors.types, fontStyle: "italic" }
        },
        {
          scope: ["storage.type.mlx", "storage.modifier.mlx"],
          settings: { foreground: colors.storage, fontStyle: "bold" }
        },
        {
          scope: ["entity.name.function.mlx"],
          settings: { foreground: colors.functions }
        },
        {
          scope: ["support.function.builtin.mlx"],
          settings: { foreground: colors.builtins, fontStyle: "bold" }
        },
        {
          scope: ["variable.parameter.mlx"],
          settings: { foreground: colors.parameters }
        },
        {
          scope: ["constant.numeric.mlx"],
          settings: { foreground: colors.numbers }
        },
        {
          scope: ["constant.language.mlx"],
          settings: { foreground: colors.constants, fontStyle: "bold" }
        },
        {
          scope: ["string.quoted.double.mlx"],
          settings: { foreground: colors.strings }
        },
        {
          scope: ["comment.line.double-slash.mlx"],
          settings: { foreground: colors.comments, fontStyle: "italic" }
        }
      ];

      // Merge with existing config
      const editorConfig = vscode.workspace.getConfiguration('editor');
      let currentTokenColors = editorConfig.get<any>('tokenColorCustomizations') || {};
      
      // We want to override just for the Mlx theme
      const mlxThemeConfig = currentTokenColors['[Mlx Vibrant Dark]'] || {};
      mlxThemeConfig.textMateRules = newRules;
      
      const newConfig = { ...currentTokenColors };
      newConfig['[Mlx Vibrant Dark]'] = mlxThemeConfig;

      try {
        await editorConfig.update('tokenColorCustomizations', newConfig, vscode.ConfigurationTarget.Global);
        vscode.window.showInformationMessage('Mlx Theme Colors updated successfully!');
      } catch (err: any) {
        vscode.window.showErrorMessage('Failed to update colors: ' + err.message);
      }
    }
  }, undefined, context.subscriptions);
}

function getWebviewContent(colors: any) {
  return `
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Mlx Theme Configurator</title>
  <style>
    body {
      font-family: var(--vscode-font-family);
      color: var(--vscode-editor-foreground);
      background-color: var(--vscode-editor-background);
      padding: 20px;
    }
    .grid {
      display: grid;
      grid-template-columns: 200px 100px;
      gap: 15px;
      margin-bottom: 20px;
    }
    .grid-item {
      display: flex;
      align-items: center;
    }
    input[type="color"] {
      border: 1px solid var(--vscode-input-border);
      background: none;
      width: 40px;
      height: 30px;
      padding: 0;
      cursor: pointer;
    }
    button {
      background-color: var(--vscode-button-background);
      color: var(--vscode-button-foreground);
      border: none;
      padding: 10px 20px;
      font-size: 14px;
      cursor: pointer;
    }
    button:hover {
      background-color: var(--vscode-button-hoverBackground);
    }
    h1 {
      font-size: 1.5em;
      margin-bottom: 20px;
    }
  </style>
</head>
<body>
  <h1>Mlx Vibrant Dark Configurator</h1>
  <p>Pick your favorite colors for the Mlx language below. Click Apply to save globally.</p>
  
  <form id="colorForm">
    <div class="grid">
      <div class="grid-item"><label for="keywords">Control Keywords (if, while)</label></div>
      <div class="grid-item"><input type="color" id="keywords" value="${colors.keywords}"></div>

      <div class="grid-item"><label for="storage">Storage Types (const, var)</label></div>
      <div class="grid-item"><input type="color" id="storage" value="${colors.storage}"></div>

      <div class="grid-item"><label for="types">Types (u32, bool)</label></div>
      <div class="grid-item"><input type="color" id="types" value="${colors.types}"></div>

      <div class="grid-item"><label for="functions">Functions</label></div>
      <div class="grid-item"><input type="color" id="functions" value="${colors.functions}"></div>

      <div class="grid-item"><label for="builtins">Builtins (@print)</label></div>
      <div class="grid-item"><input type="color" id="builtins" value="${colors.builtins}"></div>

      <div class="grid-item"><label for="parameters">Parameters</label></div>
      <div class="grid-item"><input type="color" id="parameters" value="${colors.parameters}"></div>

      <div class="grid-item"><label for="strings">Strings</label></div>
      <div class="grid-item"><input type="color" id="strings" value="${colors.strings}"></div>

      <div class="grid-item"><label for="numbers">Numbers</label></div>
      <div class="grid-item"><input type="color" id="numbers" value="${colors.numbers}"></div>
      
      <div class="grid-item"><label for="constants">Constants (true, false)</label></div>
      <div class="grid-item"><input type="color" id="constants" value="${colors.constants}"></div>

      <div class="grid-item"><label for="operators">Operators (+, -, =)</label></div>
      <div class="grid-item"><input type="color" id="operators" value="${colors.operators}"></div>

      <div class="grid-item"><label for="comments">Comments</label></div>
      <div class="grid-item"><input type="color" id="comments" value="${colors.comments}"></div>
    </div>
    
    <button type="submit">Save & Apply Colors</button>
  </form>

  <script>
    const vscode = acquireVsCodeApi();
    
    document.getElementById('colorForm').addEventListener('submit', (e) => {
      e.preventDefault();
      const colors = {
        keywords: document.getElementById('keywords').value,
        storage: document.getElementById('storage').value,
        types: document.getElementById('types').value,
        functions: document.getElementById('functions').value,
        builtins: document.getElementById('builtins').value,
        parameters: document.getElementById('parameters').value,
        strings: document.getElementById('strings').value,
        numbers: document.getElementById('numbers').value,
        constants: document.getElementById('constants').value,
        operators: document.getElementById('operators').value,
        comments: document.getElementById('comments').value
      };
      
      vscode.postMessage({
        command: 'saveColors',
        colors: colors
      });
    });
  </script>
</body>
</html>
  `;
}
