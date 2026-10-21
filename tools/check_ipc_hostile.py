# Hostile clients against mlx-ipcd (tools/check_ipc.sh): each sends what a
# well-behaved client never would, in MLXIPC's native format (std.ipc) or
# in D-Bus, and must be dropped by the bus, which goes on serving the
# others. Then connections that never say Hello must be dropped once the
# bus's --auth-timeout passed.
#
#   python3 tools/check_ipc_hostile.py BUS_SOCKET AUTH_TIMEOUT_SECONDS
import os, socket, struct, sys, time

path, auth_timeout = sys.argv[1], float(sys.argv[2])
failures = 0

def connect():
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect(path)
    return s

def dropped(s, what):
    """The bus closed the connection (recv gives EOF) within a moment."""
    global failures
    try:
        data = s.recv(4096)
        while data:
            data = s.recv(4096)
        print("ok   dropped:", what)
    except socket.timeout:
        print("FAIL not dropped:", what)
        failures += 1
    finally:
        s.close()

# --- the native format ------------------------------------------------------

PREAMBLE = b"MLXI\x01\x01\x00\x00"

def native(kind, serial, fields, body=b"", flags=0, fds=0, reply_serial=0, reserved=0):
    f = b"".join(struct.pack("<BH", code, len(value)) + value for code, value in fields)
    size = 20 + len(f) + len(body)
    return struct.pack("<IBBHIIHH", size, kind, flags, fds, serial, reply_serial, len(f), reserved) + f + body

def nstring(text):
    return struct.pack("<I", len(text)) + text + b"\x00"

HELLO = native(1, 1, [(1, b"/org/freedesktop/DBus"), (2, b"org.freedesktop.DBus"), (3, b"Hello"), (6, b"org.freedesktop.DBus")])
LIST_NAMES = native(1, 2, [(1, b"/org/freedesktop/DBus"), (2, b"org.freedesktop.DBus"), (3, b"ListNames"), (6, b"org.freedesktop.DBus")])

def native_session():
    """A native connection past its Hello."""
    s = connect()
    s.sendall(PREAMBLE + HELLO)
    data = b""
    while len(data) < 8 + 20:
        data += s.recv(4096)
    assert data[:8] == PREAMBLE, data[:8]
    size = struct.unpack("<I", data[8:12])[0]
    while len(data) < 8 + size:
        data += s.recv(4096)
    assert data[12] == 2, "no reply to Hello"
    return s

def native_rejects(what, message):
    s = native_session()
    s.sendall(message)
    dropped(s, "native: " + what)

ECHO = [(1, b"/"), (2, b"org.mlx.Echo"), (3, b"Echo"), (6, b"org.example.Echo"), (8, b"s")]

native_rejects("a member with a space", native(1, 2, [(1, b"/"), (2, b"org.mlx.Echo"), (3, b"Bad Name"), (6, b"org.example.Echo")]))
native_rejects("a path without its slash", native(1, 2, [(1, b"no/slash"), (2, b"org.mlx.Echo"), (3, b"Echo"), (6, b"org.example.Echo")]))
native_rejects("an interface of one element", native(1, 2, [(1, b"/"), (2, b"Echo"), (3, b"Echo"), (6, b"org.example.Echo")]))
native_rejects("a destination starting with a digit", native(1, 2, [(1, b"/"), (2, b"org.mlx.Echo"), (3, b"Echo"), (6, b"1org.example")]))
native_rejects("a field twice", native(1, 2, [(1, b"/"), (3, b"Echo"), (3, b"Echo"), (6, b"org.example.Echo")]))
native_rejects("an empty field", native(1, 2, [(1, b"/"), (3, b"Echo"), (2, b""), (6, b"org.example.Echo")]))
native_rejects("a reserved word that is not zero", native(1, 2, [(1, b"/"), (3, b"Echo"), (6, b"org.example.Echo")], reserved=1))
native_rejects("a serial of zero", native(1, 0, [(1, b"/"), (3, b"Echo"), (6, b"org.example.Echo")]))
native_rejects("a reply without its serial", native(2, 2, [(6, b":1.1")]))
native_rejects("a signature that is not well formed", native(1, 2, [(1, b"/"), (3, b"Echo"), (6, b"org.example.Echo"), (8, b"a{vs}")]))
native_rejects("a string with a NUL inside", native(1, 2, ECHO, nstring(b"ab\x00cd")))
native_rejects("a string that is not UTF-8", native(1, 2, ECHO, nstring(b"\xff\xfe")))
native_rejects("a string of a surrogate", native(1, 2, ECHO, nstring(b"\xed\xa0\x80")))
native_rejects("a string without its NUL", native(1, 2, ECHO, struct.pack("<I", 2) + b"abX"))
native_rejects("a body longer than its signature", native(1, 2, ECHO, nstring(b"ab") + b"\x00\x00"))
native_rejects("a body shorter than its signature", native(1, 2, ECHO, struct.pack("<I", 200)))
native_rejects("a boolean of 2", native(1, 2, [(1, b"/"), (3, b"Echo"), (6, b"org.example.Echo"), (8, b"b")], b"\x02"))
native_rejects("a descriptor index past the message's", native(1, 2, [(1, b"/"), (3, b"Echo"), (6, b"org.example.Echo"), (8, b"h")], struct.pack("<I", 0)))
native_rejects("a variant of two types", native(1, 2, [(1, b"/"), (3, b"Echo"), (6, b"org.example.Echo"), (8, b"v")], b"\x02ss\x00" + nstring(b"a") + nstring(b"b")))
native_rejects("an object path value without its slash", native(1, 2, [(1, b"/"), (3, b"Echo"), (6, b"org.example.Echo"), (8, b"o")], nstring(b"bad")))
native_rejects("an array of more elements than bytes", native(1, 2, [(1, b"/"), (3, b"Echo"), (6, b"org.example.Echo"), (8, b"ay")], struct.pack("<I", 1000) + b"\x01"))
native_rejects("a message of 70000 bytes", struct.pack("<I", 70000) + b"\x01" * 16)
native_rejects("a message smaller than its header", struct.pack("<I", 8) + b"\x00" * 16)
native_rejects("a kind of 9", native(9, 2, [(1, b"/"), (3, b"Echo")]))
native_rejects("a call with a reply serial", native(1, 2, [(1, b"/"), (3, b"Echo"), (6, b"org.example.Echo")], reply_serial=7))
native_rejects("more descriptors than the message has", native(1, 2, ECHO, nstring(b"x"), fds=3))

# Before Hello: another message first, a preamble of a version not spoken,
# a preamble of someone else.
s = connect(); s.sendall(PREAMBLE + LIST_NAMES); dropped(s, "native: a first message that is not Hello")
s = connect(); s.sendall(b"MLXI\x07\x01\x00\x00" + HELLO); dropped(s, "native: version 7")
s = connect(); s.sendall(b"MLXQ\x01\x01\x00\x00" + HELLO); dropped(s, "native: a magic of someone else")
s = connect(); s.sendall(b"GET / HTTP/1.0\r\n\r\n"); dropped(s, "neither NUL nor preamble")

# --- D-Bus ------------------------------------------------------------------

def dbus_string(text):
    return struct.pack("<I", len(text)) + text + b"\x00"

def pad8(b):
    return b + b"\x00" * (-len(b) % 8)

def dbus(kind, serial, fields, body=b"", flags=0, fds=None):
    """fields: (code, type letter, value); strings and paths as bytes, u32 as int."""
    out = b""
    for code, letter, value in fields:
        out = pad8(out)
        out += struct.pack("<BB", code, 1) + letter + b"\x00"
        if letter in (b"s", b"o"):
            out += dbus_string(value)
        elif letter == b"g":
            out += struct.pack("<B", len(value)) + value + b"\x00"
        else:
            out += struct.pack("<I", value)
    header = struct.pack("<cBBBIII", b"l", kind, flags, 1, len(body), serial, len(out)) + out
    return pad8(header) + body

DBUS_HELLO = dbus(1, 1, [(1, b"o", b"/org/freedesktop/DBus"), (2, b"s", b"org.freedesktop.DBus"), (3, b"s", b"Hello"), (6, b"s", b"org.freedesktop.DBus")])

def dbus_session():
    s = connect()
    uid = str(os.getuid()).encode().hex().encode()
    s.sendall(b"\x00AUTH EXTERNAL " + uid + b"\r\nBEGIN\r\n")
    line = b""
    while not line.endswith(b"\r\n"):
        line += s.recv(1)
    assert line.startswith(b"OK "), line
    s.sendall(DBUS_HELLO)
    data = b""
    while len(data) < 16:
        data += s.recv(4096)
    assert data[1] == 2, "no reply to Hello"
    body = struct.unpack("<I", data[4:8])[0]
    fields = struct.unpack("<I", data[12:16])[0]
    size = (16 + fields + 7) // 8 * 8 + body
    while len(data) < size:
        data += s.recv(4096)
    return s

def dbus_rejects(what, message):
    s = dbus_session()
    s.sendall(message)
    dropped(s, "D-Bus: " + what)

DBUS_ECHO = [(1, b"o", b"/"), (2, b"s", b"org.mlx.Echo"), (3, b"s", b"Echo"), (6, b"s", b"org.example.Echo"), (8, b"g", b"s")]
dbus_rejects("a member with a space", dbus(1, 2, [(1, b"o", b"/"), (2, b"s", b"org.mlx.Echo"), (3, b"s", b"Bad Name"), (6, b"s", b"org.example.Echo")]))
dbus_rejects("a path without its slash", dbus(1, 2, [(1, b"o", b"no/slash"), (3, b"s", b"Echo"), (6, b"s", b"org.example.Echo")]))
dbus_rejects("a string that is not UTF-8", dbus(1, 2, DBUS_ECHO, dbus_string(b"\xff\xfe")))
dbus_rejects("a string with a NUL inside", dbus(1, 2, DBUS_ECHO, dbus_string(b"a\x00b")))
dbus_rejects("a boolean of 2", dbus(1, 2, DBUS_ECHO[:4] + [(8, b"g", b"b")], struct.pack("<I", 2)))
dbus_rejects("a descriptor index past the message's", dbus(1, 2, DBUS_ECHO[:4] + [(8, b"g", b"h")], struct.pack("<I", 0)))
dbus_rejects("a body longer than its signature", dbus(1, 2, DBUS_ECHO, dbus_string(b"ab") + b"\x00\x00\x00\x00"))
dbus_rejects("an array past the body", dbus(1, 2, DBUS_ECHO[:4] + [(8, b"g", b"ay")], struct.pack("<I", 1000) + b"\x01"))
dbus_rejects("a well-known destination starting with a digit", dbus(1, 2, [(1, b"o", b"/"), (3, b"s", b"Echo"), (6, b"s", b"1org.example")]))

# --- the bus still answers everyone else ------------------------------------

s = native_session()
s.sendall(LIST_NAMES)
data = b""
while len(data) < 20:
    data += s.recv(4096)
size = struct.unpack("<I", data[:4])[0]
while len(data) < size:
    data += s.recv(4096)
kind, flags, fds, serial, reply_serial, flen = struct.unpack("<BBHIIH", data[4:18])
assert kind == 2 and reply_serial == 2, (kind, reply_serial)
body = data[20 + flen:]
count = struct.unpack("<I", body[:4])[0]
assert count >= 1, count
s.close()
print("ok   the bus still answers after all that")

# --- connections that never say Hello ---------------------------------------

idle = connect()
preamble_only = connect(); preamble_only.sendall(PREAMBLE)
nul_only = connect(); nul_only.sendall(b"\x00")
time.sleep(auth_timeout + 1.5)
for s, what in ((idle, "nothing sent"), (preamble_only, "the preamble alone"), (nul_only, "the NUL alone")):
    s.settimeout(2)
    dropped(s, "no Hello in time: " + what)

sys.exit(1 if failures else 0)
