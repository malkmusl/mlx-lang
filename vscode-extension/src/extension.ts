import * as vscode from 'vscode';
import { LanguageClient, LanguageClientOptions, ServerOptions, TransportKind } from 'vscode-languageclient/node';
import { getLspBinaryPath } from './client';
import { showAstPanel } from './astViewer';
import { showColorConfigurator } from './colorConfigurator';

let client: LanguageClient | undefined;

export function activate(context: vscode.ExtensionContext) {
  const serverPath = getLspBinaryPath(context);
  const serverOptions: ServerOptions = {
    run: { command: serverPath, transport: TransportKind.stdio },
    debug: { command: serverPath, transport: TransportKind.stdio }
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: 'file', language: 'mlx' }],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher('**/.clientrc')
    },
    middleware: {
      provideDocumentSemanticTokens: async (document, token, next) => {
        return await next(document, token);
      }
    }
  };

  client = new LanguageClient(
    'mlxLanguageServer',
    'Mlx Language Server',
    serverOptions,
    clientOptions
  );

  client.start();

  const astCommand = vscode.commands.registerCommand('mlx.showAst', async () => {
    if (!client) { return; }
    const editor = vscode.window.activeTextEditor;
    if (!editor) { return; }
    const uri = editor.document.uri.toString();
    const astResult = await client.sendRequest('mlx/ast', { uri });
    showAstPanel(context, astResult as string);
  });

  const customizeColorsCommand = vscode.commands.registerCommand('mlx.customizeColors', () => {
    showColorConfigurator(context);
  });

  context.subscriptions.push(astCommand, customizeColorsCommand);
}

export function deactivate(): Thenable<void> | undefined {
  if (!client) { return undefined; }
  return client.stop();
}
