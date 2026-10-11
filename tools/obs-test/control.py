#!/usr/bin/env python3
"""Switches an MLX Capture source between targets while OBS runs, as a
user does in its properties, for tools/check_obs_plugin.sh: a small
obs-websocket (protocol 5, no authentication) client on the standard
library.

    control.py PORT SOURCE TARGET...

For each TARGET ("" the screen, else a window's app id) it asks for the
source's list of targets (the plugin's get_properties, on another thread
than OBS's video thread) and sets the source's "target" (its update: the
plugin starts over on the new target), one second apart, and prints a
line for each."""
import base64, json, os, socket, struct, sys, time

def connect(port):
    for _ in range(100):
        try:
            sock = socket.create_connection(('127.0.0.1', port), timeout=5)
            break
        except OSError:
            time.sleep(0.2)
    else:
        sys.exit('no obs-websocket on port %d' % port)
    key = base64.b64encode(os.urandom(16)).decode()
    sock.sendall(('GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                  'Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: obswebsocket.json\r\n\r\n' % key).encode())
    reply = b''
    while b'\r\n\r\n' not in reply:
        reply += sock.recv(4096)
    assert b' 101 ' in reply.split(b'\r\n')[0], reply
    return sock, reply.split(b'\r\n\r\n', 1)[1]

class Client:
    def __init__(self, port):
        self.sock, self.buffer = connect(port)
    def send(self, message):
        data = json.dumps(message).encode()
        mask = os.urandom(4)
        header = bytes([0x81])
        if len(data) < 126: header += bytes([0x80 | len(data)])
        elif len(data) < 65536: header += bytes([0x80 | 126]) + struct.pack('>H', len(data))
        else: header += bytes([0x80 | 127]) + struct.pack('>Q', len(data))
        self.sock.sendall(header + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))
    def read(self, count):
        while len(self.buffer) < count:
            chunk = self.sock.recv(65536)
            if not chunk: raise EOFError('OBS closed the connection')
            self.buffer += chunk
        data, self.buffer = self.buffer[:count], self.buffer[count:]
        return data
    def receive(self):
        while True:
            first, second = self.read(2)
            length = second & 127
            if length == 126: length = struct.unpack('>H', self.read(2))[0]
            elif length == 127: length = struct.unpack('>Q', self.read(8))[0]
            payload = self.read(length)
            if first & 15 == 1: return json.loads(payload)
    def request(self, kind, data, number):
        self.send({'op': 6, 'd': {'requestType': kind, 'requestId': str(number), 'requestData': data}})
        while True:
            message = self.receive()
            if message['op'] == 7 and message['d']['requestId'] == str(number): return message['d']

port, source, targets = int(sys.argv[1]), sys.argv[2], sys.argv[3:]
client = Client(port)
assert client.receive()['op'] == 0
client.send({'op': 1, 'd': {'rpcVersion': 1, 'eventSubscriptions': 0}})
assert client.receive()['op'] == 2
number = 0
for target in targets:
    number += 1
    items = client.request('GetInputPropertiesListPropertyItems', {'inputName': source, 'propertyName': 'target'}, number)
    names = [item['itemValue'] for item in items.get('responseData', {}).get('propertyItems', [])]
    number += 1
    result = client.request('SetInputSettings', {'inputName': source, 'inputSettings': {'target': target}}, number)
    print('target %r: %s; list %s' % (target, 'ok' if result['requestStatus']['result'] else result['requestStatus'], names), flush=True)
    time.sleep(1)
