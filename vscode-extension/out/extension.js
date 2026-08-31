"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const vscode = __importStar(require("vscode"));
const node_1 = require("vscode-languageclient/node");
const client_1 = require("./client");
const astViewer_1 = require("./astViewer");
const colorConfigurator_1 = require("./colorConfigurator");
let client;
function activate(context) {
    const serverPath = (0, client_1.getLspBinaryPath)(context);
    const serverOptions = {
        run: { command: serverPath, transport: node_1.TransportKind.stdio },
        debug: { command: serverPath, transport: node_1.TransportKind.stdio }
    };
    const clientOptions = {
        documentSelector: [{ scheme: 'file', language: 'zin' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/.clientrc')
        },
        middleware: {
            provideDocumentSemanticTokens: async (document, token, next) => {
                return await next(document, token);
            }
        }
    };
    client = new node_1.LanguageClient('zinLanguageServer', 'Zin Language Server', serverOptions, clientOptions);
    client.start();
    const astCommand = vscode.commands.registerCommand('zin.showAst', async () => {
        if (!client) {
            return;
        }
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            return;
        }
        const uri = editor.document.uri.toString();
        const astResult = await client.sendRequest('zin/ast', { uri });
        (0, astViewer_1.showAstPanel)(context, astResult);
    });
    const customizeColorsCommand = vscode.commands.registerCommand('zin.customizeColors', () => {
        (0, colorConfigurator_1.showColorConfigurator)(context);
    });
    context.subscriptions.push(astCommand, customizeColorsCommand);
}
function deactivate() {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
//# sourceMappingURL=extension.js.map