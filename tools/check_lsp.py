#!/usr/bin/env python3
# A scripted editor session against mlx-lsp (tools/mlx-lsp), on a small tree
# of its own (two files, one importing the other): every request the server
# offers, and the compiler's diagnostics as a file is saved broken and
# fixed. Run by tools/check_lsp.sh.
#
#   tools/check_lsp.py SERVER COMPILER
import json, os, select, shutil, subprocess, sys, tempfile, time

SERVER, COMPILER = sys.argv[1], sys.argv[2]

SHAPES = '''// Shapes and what they measure.

// A point in the plane.
pub const Point = struct {
    x: i32
    y: i32

    // The distance to the origin, squared.
    pub fn lengthSquared(point: *const Self) -> i32 {
        return point.*.x * point.*.x + point.*.y * point.*.y
    }
}

// Adds two numbers, `first` then `second`.
pub fn add(first: i32, second: i32) -> i32 {
    return first + second
}

pub const origin_x: i32 = 0

fn hidden() -> i32 { return 1 }

fn dropping() -> void {
    const ignored = hidden()
}
'''

MAIN = '''const shapes = @import("../lib/shapes.mlx")

fn measure(point: *const shapes.Point) -> i32 {
    const total = shapes.add(point.*.x, point.*.y)
    return total + shapes.origin_x
}

pub fn main() -> u8 {
    var point = shapes.Point.{ .x = 3, .y = 4 }
    const size = measure(&point)
    return @intCast(u8, size)
}
'''


class Client:
    def __init__(self, root):
        self.log = open(os.path.join(root, '..', 'server.log'), 'w')
        env = dict(os.environ, MLX_COMPILER=COMPILER, XDG_STATE_HOME=os.path.join(root, '..', 'state'))
        self.process = subprocess.Popen([SERVER], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log, env=env)
        self.buffer = b''
        self.id = 0
        self.notes = []

    def send(self, message):
        body = json.dumps(message).encode()
        self.process.stdin.write(b'Content-Length: %d\r\n\r\n' % len(body) + body)
        self.process.stdin.flush()

    def read(self, timeout):
        end = time.time() + timeout
        while True:
            if b'\r\n\r\n' in self.buffer:
                head, rest = self.buffer.split(b'\r\n\r\n', 1)
                length = int(head.split(b':')[1])
                if len(rest) >= length:
                    self.buffer = rest[length:]
                    return json.loads(rest[:length])
            ready, _, _ = select.select([self.process.stdout], [], [], max(0, end - time.time()))
            if not ready:
                return None
            chunk = os.read(self.process.stdout.fileno(), 65536)
            if not chunk:
                return None
            self.buffer += chunk

    def request(self, method, params, timeout=60):
        self.id += 1
        self.send({'jsonrpc': '2.0', 'id': self.id, 'method': method, 'params': params})
        while True:
            message = self.read(timeout)
            if message is None:
                raise SystemExit('FAIL no answer to ' + method)
            if message.get('id') == self.id:
                return message
            self.notes.append(message)

    def notify(self, method, params):
        self.send({'jsonrpc': '2.0', 'method': method, 'params': params})

    def diagnostics(self, uri, timeout=60):
        for at, note in enumerate(self.notes):
            if note.get('method') == 'textDocument/publishDiagnostics' and note['params']['uri'] == uri:
                return self.notes.pop(at)['params']['diagnostics']
        end = time.time() + timeout
        while time.time() < end:
            message = self.read(end - time.time())
            if message is None:
                return None
            if message.get('method') == 'textDocument/publishDiagnostics' and message['params']['uri'] == uri:
                return message['params']['diagnostics']
            self.notes.append(message)
        return None


def position(text, needle, offset=0, occurrence=1):
    at = -1
    for _ in range(occurrence):
        at = text.index(needle, at + 1)
    at += offset
    return {'line': text.count('\n', 0, at), 'character': at - (text.rfind('\n', 0, at) + 1)}


failures = []


def check(name, condition, detail=None):
    if condition:
        print('ok   lsp: ' + name)
    else:
        print('FAIL lsp: ' + name + ('  ' + json.dumps(detail)[:800] if detail is not None else ''))
        failures.append(name)


work = tempfile.mkdtemp()
root = os.path.join(work, 'tree')
os.makedirs(os.path.join(root, 'lib'))
os.makedirs(os.path.join(root, 'app'))
shapes_path = os.path.join(root, 'lib', 'shapes.mlx')
main_path = os.path.join(root, 'app', 'main.mlx')
open(shapes_path, 'w').write(SHAPES)
open(main_path, 'w').write(MAIN)
shapes_uri = 'file://' + shapes_path
main_uri = 'file://' + main_path
# A crash the desktop programs kept (examples/wayland-compositor/crash.mlx):
# in add, line 16, called from measure.
os.makedirs(os.path.join(work, 'state', 'mlx'))
open(os.path.join(work, 'state', 'mlx', 'crashes.log'), 'w').write('crash\t1790000000\tapp\tillegal instruction (a failed runtime check)\tlib/shapes.mlx:15:add+0x10 at lib/shapes.mlx:16\tapp/main.mlx:3:measure+0x20 at app/main.mlx:4\n')
client = Client(root)
try:
    result = client.request('initialize', {'processId': None, 'rootUri': 'file://' + root, 'capabilities': {}})['result']
    capabilities = result['capabilities']
    check('initialize offers the features', all(capabilities.get(name) for name in ('hoverProvider', 'definitionProvider', 'referencesProvider', 'renameProvider', 'completionProvider', 'signatureHelpProvider', 'semanticTokensProvider', 'documentSymbolProvider', 'workspaceSymbolProvider', 'documentHighlightProvider', 'codeLensProvider')), capabilities)
    client.notify('initialized', {})
    client.notify('textDocument/didOpen', {'textDocument': {'uri': main_uri, 'languageId': 'mlx', 'version': 1, 'text': MAIN}})
    main = {'uri': main_uri}

    answer = client.request('textDocument/definition', {'textDocument': main, 'position': position(MAIN, 'shapes.add', 8)})['result']
    check('definition of shapes.add: its declaration', answer and answer['uri'] == shapes_uri and answer['range']['start'] == position(SHAPES, 'add(first'), answer)
    answer = client.request('textDocument/definition', {'textDocument': main, 'position': position(MAIN, 'shapes = @import', 2)})['result']
    check('definition of an import\'s name: the file', answer and answer['uri'] == shapes_uri and answer['range']['start']['line'] == 0, answer)
    answer = client.request('textDocument/definition', {'textDocument': main, 'position': position(MAIN, 'point.*.x', 8)})['result']
    check('definition of a field through a pointer', answer and answer['uri'] == shapes_uri and answer['range']['start'] == position(SHAPES, 'x: i32'), answer)

    answer = client.request('textDocument/references', {'textDocument': main, 'position': position(MAIN, 'shapes.add', 8), 'context': {'includeDeclaration': True}})['result']
    check('references of add: the declaration and the call', answer and sorted(item['uri'] for item in answer) == sorted([main_uri, shapes_uri]), answer)

    answer = client.request('textDocument/hover', {'textDocument': main, 'position': position(MAIN, 'shapes.add', 8)})['result']
    text = answer['contents']['value'] if answer else ''
    check('hover of add: signature, doc comment, place', 'pub fn add(first: i32, second: i32) -> i32' in text and 'Adds two numbers' in text and 'lib/shapes.mlx:15' in text, text)
    answer = client.request('textDocument/hover', {'textDocument': main, 'position': position(MAIN, 'return total', 7)})['result']
    text = answer['contents']['value'] if answer else ''
    check('hover of a local: its declaration', 'const total = shapes.add' in text, text)

    client.notify('textDocument/didOpen', {'textDocument': {'uri': shapes_uri, 'languageId': 'mlx', 'version': 1, 'text': SHAPES}})
    answer = client.request('textDocument/documentSymbol', {'textDocument': {'uri': shapes_uri}})['result']
    names = [item['name'] for item in answer or []]
    point = [item for item in answer or [] if item['name'] == 'Point']
    children = [item['name'] for item in point[0].get('children', [])] if point else []
    check('outline: Point with x, y, lengthSquared; add; origin_x', names[:3] == ['Point', 'add', 'origin_x'] and children == ['x', 'y', 'lengthSquared'], answer)
    check('outline: Point\'s range is its whole body', point and point[0]['range']['end']['line'] == 11, point)

    answer = client.request('workspace/symbol', {'query': 'length'})['result']
    check('workspace symbols: lengthSquared', answer and any(item['name'] == 'lengthSquared' and item['location']['uri'] == shapes_uri for item in answer), answer)

    answer = client.request('textDocument/documentHighlight', {'textDocument': main, 'position': position(MAIN, 'point: *const', 1)})['result']
    check('highlights of point in measure', answer and len(answer) == 3, answer)

    # Completion: members after a dot (the module's public ones; a struct's
    # fields and functions through a pointer), what is in scope.
    typed = MAIN.replace('    return total + shapes.origin_x\n', '    shapes.\n    point.*.\n    return total + shapes.origin_x\n')
    client.notify('textDocument/didChange', {'textDocument': {'uri': main_uri, 'version': 2}, 'contentChanges': [{'text': typed}]})
    labels = [item['label'] for item in client.request('textDocument/completion', {'textDocument': main, 'position': position(typed, 'shapes.\n', 7)})['result']['items']]
    check('completion after shapes.: its public declarations', sorted(labels) == ['Point', 'add', 'origin_x'], labels)
    labels = [item['label'] for item in client.request('textDocument/completion', {'textDocument': main, 'position': position(typed, 'point.*.\n', 8)})['result']['items']]
    check('completion after point.*.: the fields and functions of Point', sorted(labels) == ['lengthSquared', 'x', 'y'], labels)
    labels = [item['label'] for item in client.request('textDocument/completion', {'textDocument': main, 'position': position(typed, '    shapes.\n', 4)})['result']['items']]
    check('completion in scope: parameter, local, declarations, keywords', all(name in labels for name in ('point', 'total', 'measure', 'shapes', 'return')), labels)
    client.notify('textDocument/didChange', {'textDocument': {'uri': main_uri, 'version': 3}, 'contentChanges': [{'text': MAIN}]})

    answer = client.request('textDocument/signatureHelp', {'textDocument': main, 'position': position(MAIN, 'point.*.y)', 0)})['result']
    check('signature help: add, the second parameter', answer and answer['signatures'][0]['label'] == 'pub fn add(first: i32, second: i32) -> i32' and answer['activeParameter'] == 1 and len(answer['signatures'][0]['parameters']) == 2, answer)

    answer = client.request('textDocument/prepareRename', {'textDocument': main, 'position': position(MAIN, 'total =', 1)})['result']
    check('prepare rename of a local', answer and answer['placeholder'] == 'total', answer)
    answer = client.request('textDocument/rename', {'textDocument': main, 'position': position(MAIN, 'total =', 1), 'newName': 'sum'})['result']
    edits = answer['changes'].get(main_uri, []) if answer else []
    check('rename of a local: its declaration and use', len(edits) == 2 and list(answer['changes']) == [main_uri], answer)
    answer = client.request('textDocument/rename', {'textDocument': main, 'position': position(MAIN, 'shapes.add', 8), 'newName': 'plus'})['result']
    check('rename of add: in both files', answer and sorted(answer['changes']) == sorted([main_uri, shapes_uri]), answer)
    answer = client.request('textDocument/rename', {'textDocument': main, 'position': position(MAIN, 'shapes.add', 8), 'newName': '9lives'})
    check('rename to something that is not a name: refused', 'error' in answer, answer)

    data = client.request('textDocument/semanticTokens/full', {'textDocument': main})['result']['data']
    tokens = []
    line = column = 0
    for at in range(0, len(data), 5):
        line += data[at]
        column = data[at + 1] if data[at] else column + data[at + 1]
        tokens.append((line, column, data[at + 2], data[at + 3]))
    kinds = {(line, column): kind for line, column, width, kind in tokens}
    check('semantic tokens: keyword, namespace, function, parameter, property', kinds.get((0, 0)) == 15 and kinds.get((0, 6)) == 0 and kinds.get((2, 3)) == 12 and kinds.get((2, 11)) == 7 and kinds.get((3, 37)) == 9, tokens[:12])

    # What the code map knows: in hover, code lens and diagnostics.
    answer = client.request('textDocument/hover', {'textDocument': main, 'position': position(MAIN, 'shapes.add', 8)})['result']
    text = answer['contents']['value'] if answer else ''
    check('hover of add: the crash, complexity, the program and no check', '**Crashed** here 1x' in text and 'Complexity: 1 branches' in text and 'Reached by app' in text and 'no check reaches it' in text, text)
    client.notify('textDocument/didOpen', {'textDocument': {'uri': shapes_uri, 'languageId': 'mlx', 'version': 1, 'text': SHAPES}})
    shapes = {'uri': shapes_uri}
    answer = client.request('textDocument/hover', {'textDocument': shapes, 'position': position(SHAPES, 'hidden()')})['result']
    text = answer['contents']['value'] if answer else ''
    check('hover of hidden: unreachable', '**Unreachable:**' in text, text)
    answer = client.request('textDocument/codeLens', {'textDocument': shapes})['result']
    titles = {(item['range']['start']['line']): item['command']['title'] for item in (answer or [])}
    check('code lens: add used once, crashed once, no check', titles.get(position(SHAPES, 'add(first')['line']) == '1 use · 1 crash · no check', answer)
    check('code lens: dropping has a workaround and is unreachable', titles.get(position(SHAPES, 'dropping()')['line']) == '0 uses · 1 workaround · unreachable', answer)
    listed = client.diagnostics(shapes_uri)
    crashed = [item for item in listed or [] if item['severity'] == 2]
    hinted = [item for item in listed or [] if item['severity'] == 4]
    check('diagnostics: a warning on the line the crash stopped on', crashed and crashed[0]['range']['start']['line'] == 15 and 'Crashed here: app' in crashed[0]['message'], listed)
    check('diagnostics: a hint on the dropped result', hinted and hinted[0]['range']['start']['line'] == position(SHAPES, 'ignored')['line'] and 'Results bound to be dropped' in hinted[0]['message'], listed)

    # Diagnostics: clean, then saved broken, then fixed.
    listed = client.diagnostics(main_uri)
    through = [item for item in listed or [] if item.get('source') == 'mlx-codemap']
    check('diagnostics: a warning on the call a crash went through', through and through[0]['range']['start']['line'] == 3 and 'A crash went through this call' in through[0]['message'], listed)
    listed = [item for item in listed or [] if item.get('source') != 'mlx-codemap']
    check('diagnostics: none for a clean file', listed == [], listed)
    open(main_path, 'w').write(MAIN.replace('return total +', 'return totl +'))
    client.notify('textDocument/didChange', {'textDocument': {'uri': main_uri, 'version': 4}, 'contentChanges': [{'text': MAIN.replace('return total +', 'return totl +')}]})
    client.notify('textDocument/didSave', {'textDocument': main})
    listed = [item for item in client.diagnostics(main_uri) or [] if item.get('source') != 'mlx-codemap']
    check('diagnostics: the compiler\'s error where it is, with the name nothing declares', listed and listed[0]['range']['start']['line'] == position(MAIN, 'total +')['line'] and 'not declared: `totl`' in listed[0]['message'] and listed[0]['severity'] == 1, listed)
    open(main_path, 'w').write(MAIN)
    client.notify('textDocument/didChange', {'textDocument': {'uri': main_uri, 'version': 5}, 'contentChanges': [{'text': MAIN}]})
    client.notify('textDocument/didSave', {'textDocument': main})
    listed = [item for item in client.diagnostics(main_uri) or [] if item.get('source') != 'mlx-codemap']
    check('diagnostics: cleared once fixed', listed == [], listed)

    client.request('shutdown', None)
    client.notify('exit', None)
    client.process.wait(timeout=10)
    check('exit after shutdown', client.process.returncode == 0, client.process.returncode)
finally:
    if client.process.poll() is None:
        client.process.kill()
    shutil.rmtree(work, ignore_errors=True)
sys.exit(1 if failures else 0)
