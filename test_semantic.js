const cp = require('child_process');
const server = cp.spawn('./vscode-extension/bin/zin-lsp', [], { stdio: ['pipe', 'pipe', 'inherit'] });

function send(msg) {
    const str = JSON.stringify(msg);
    server.stdin.write(`Content-Length: ${Buffer.byteLength(str)}\r\n\r\n${str}`);
}

server.stdout.on('data', data => console.log('OUT:', data.toString()));

send({
    jsonrpc: "2.0",
    id: 1,
    method: "initialize",
    params: { capabilities: {} }
});

send({
    jsonrpc: "2.0",
    method: "textDocument/didOpen",
    params: {
        textDocument: {
            uri: "file:///test.zin",
            text: "const foo = 1\n"
        }
    }
});

send({
    jsonrpc: "2.0",
    id: 2,
    method: "textDocument/semanticTokens/full",
    params: {
        textDocument: { uri: "file:///test.zin" }
    }
});

setTimeout(() => {
    send({ jsonrpc: "2.0", id: 3, method: "shutdown", params: null });
    send({ jsonrpc: "2.0", method: "exit", params: null });
}, 100);
