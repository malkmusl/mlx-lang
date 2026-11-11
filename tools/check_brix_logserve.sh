#!/usr/bin/env bash
# logserve (projects/brix/tools/logserve) in brixOS: the kernel log and
# the machine's state read from another computer, here the host through
# QEMU's user network (hostfwd= forwards a host port to the guest's 8080
# and 8081; the guest is 10.0.2.15 and sees the host as 10.0.2.2).
# Started from the serial shell it says where to open it; the host gets
# /, /klog (the boot's lines), /gpu (no such line on a machine without
# the card, said in a first line), /system and /all, a 404 for every
# other path (none is opened as a file), a refusal for another method,
# for a head that is too long and for what is no HTTP; a request that
# never ends and a connection that says nothing keep nobody waiting and
# are closed after the time limit; three clients at once; each request
# is a line on the terminal (a path's control characters not passed on);
# Ctrl+C ends it with 0 and the port is closed. Started again at once on
# the same port it listens (a client of the run before still keeps its
# connection); clients that got their answer and never close their side
# take the kernel's sockets only for a while (as many as it has: then no
# request is answered, and twenty seconds later the next one is). A
# request's head of 2048 bytes is read, one of 2049 refused; a head that
# comes a byte at a time; a query, a fragment or a NUL in the path lead
# to no file. With two brixfs volumes:
# /boot.log (a link of that name is not followed), /boot.previous.log
# from both (a megabyte of each at most; also for a client that closes
# its side after asking, and many times over without the machine's
# memory going), /all with them, and a client that stops reading a long
# answer (one in brixOS: tools/logserve-test/reader.mlx) is given up
# after ten seconds while the others are answered. `logserve send`
# writes the /all text to a listener on the host (one that closed its own
# side at once, as a listener without input does). Then as many requests
# one after the other as the machine's free memory has rings for the
# kernel's sockets (32 a MiB: each one is answered, and the machine has
# the memory it had). Then on an igb card
# whose cable is out at boot (no address: said; the cable and the
# address said when they have come; the pages over igb), on a machine
# with little memory (eight clients at once ask for more megabytes than
# it has: each gets the text or a 503, and logserve goes on), and on a
# machine without a network device (said, 1).
#
#   tools/check_brix_logserve.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"
    exit 0
fi
if ! command -v python3 > /dev/null; then
    echo "skip: no python3 for the host's side"
    exit 0
fi
work=$(mktemp -d)
qemu_pid=
listener_pid=
holder_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    [[ -n "$listener_pid" ]] && kill "$listener_pid" 2> /dev/null || true
    [[ -n "$holder_pid" ]] && kill "$holder_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 30 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/brix/tools/logserve/main.mlx -o "$work/logserve"
"$compiler" --quiet $brix projects/brix/tools/brixfs/main.mlx -o "$work/brixfs"
# (The client that stops reading runs in brixOS: on the host, QEMU and
# the host's own buffers take megabytes for a client that reads nothing.)
"$compiler" --quiet $brix tools/logserve-test/reader.mlx -o "$work/reader"
for program in cat echo seq sync ln; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "logserve=$work/logserve" "brixfs=$work/brixfs" "reader=$work/reader" "$work/bin/"
# (Built for Linux too: its HTTP side can be tried on the host, where
# there is no kernel log to give.)
"$compiler" --quiet projects/brix/tools/logserve/main.mlx -o "$work/logserve-linux"
echo "ok   the kernel, logserve (for brixOS, and for Linux) and a boot image build"

# The host's side: requests over sockets of its own, so that what is sent
# and when is the check's. Every command ends with 1 and a line on a miss.
cat > "$work/client.py" <<'PY'
import os, re, select, socket, sys, threading, time

HOST = "127.0.0.1"
MIB = 1 << 20
NO_PAGE = b"logserve has no such page. It has: / /all /gpu /klog /boot.log /boot.previous.log /system\n"

def fail(text):
    print("FAIL " + text, file=sys.stderr)
    sys.exit(1)

def rest(s):
    data = b""
    try:
        while True:
            got = s.recv(65536)
            if not got:
                break
            data += got
    except ConnectionResetError:
        pass
    s.close()
    return data

# A request's answer, read to the connection's end; `early`: to its
# Content-Length, as a browser reads, which then closes first.
def exchange(port, raw, early=False, timeout=60.0):
    s = socket.create_connection((HOST, port), timeout=timeout)
    s.sendall(raw)
    data = b""
    try:
        while True:
            head, sep, body = data.partition(b"\r\n\r\n")
            length = re.search(rb"\r\nContent-Length: (\d+)", head)
            if early and sep and length and len(body) >= int(length.group(1)):
                break
            got = s.recv(65536)
            if not got:
                break
            data += got
    except ConnectionResetError:
        pass
    s.close()
    return data

def split(data):
    head, sep, body = data.partition(b"\r\n\r\n")
    if not sep:
        return "", {}, b""
    lines = head.decode("latin-1").split("\r\n")
    headers = {}
    for line in lines[1:]:
        name, _, value = line.partition(":")
        headers[name.strip().lower()] = value.strip()
    return lines[0], headers, body

def get(port, path, method="GET"):
    return split(exchange(port, ("%s %s HTTP/1.1\r\nHost: brix\r\nUser-Agent: check\r\n\r\n" % (method, path)).encode("latin-1")))

def page(port, path, kind="text/plain; charset=utf-8"):
    status, headers, body = get(port, path)
    if status != "HTTP/1.1 200 OK":
        fail("%s: %r" % (path, status))
    if headers.get("content-type") != kind:
        fail("%s is %r" % (path, headers.get("content-type")))
    if headers.get("content-length") != str(len(body)):
        fail("%s: %d bytes came, Content-Length says %s" % (path, len(body), headers.get("content-length")))
    if headers.get("connection") != "close" or headers.get("cache-control") != "no-store" or headers.get("x-content-type-options") != "nosniff":
        fail("%s: headers %r" % (path, headers))
    return body

def numbers(first, last):
    return "".join("%d\n" % n for n in range(first, last + 1)).encode()

def pages(port, out):
    index = page(port, "/", "text/html; charset=utf-8").decode()
    for name in ("/all", "/gpu", "/klog", "/boot.log", "/boot.previous.log", "/system"):
        if 'href="%s"' % name not in index:
            fail("/ does not list %s" % name)
    klog = page(port, "/klog")
    for line in (b"brixOS: probing network\n", b"brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)\n"):
        if line not in klog:
            fail("/klog lacks %r" % line)
    open(out + "/klog", "wb").write(klog)
    if page(port, "/klog?since=boot")[:len(klog)] != klog:
        fail("/klog with a query is another text")
    gpu = page(port, "/gpu")
    lines = b"".join(line + b"\n" for line in klog.split(b"\n") if line.startswith(b"gpu:"))
    if lines and gpu != lines:
        fail("/gpu is not the kernel log's gpu: lines")
    if not lines and (not gpu.startswith(b'(no line of the kernel log begins with "gpu:"') or gpu.count(b"\n") != 1):
        fail("/gpu without a card: %r" % gpu[:200])
    system = page(port, "/system").decode()
    for pattern in (r"^memory: \d+ MiB, \d+ free$", r"^processors: \d+$", r"^uptime: \d+\.\d s$",
                    r"^page faults: \d+, system calls: \d+, switches: \d+$", r"^device interrupts: \d+$",
                    r"^net0: virtio, hardware address 52:54:00:12:34:56, cable connected$",
                    r"^net0: address 10\.0\.2\.15, netmask 255\.255\.255\.0, gateway 10\.0\.2\.2, name server 10\.0\.2\.3 \(by DHCP\)$",
                    r"^net0: [1-9]\d* frames received, [1-9]\d* sent$"):
        if not re.search(pattern, system, re.M):
            fail("/system lacks %s:\n%s" % (pattern, system))
    everything = page(port, "/all")
    start = b"==== /system: the machine and its network links ====\n"
    if not everything.startswith(start) or b"\nnet0: virtio, hardware address" not in everything:
        fail("/all does not begin with the machine and its links")
    mark = b'==== /gpu: the kernel log\'s lines that begin with "gpu:" (%d) ====\n' % lines.count(b"\n")
    at = everything.find(mark)
    found = re.compile(rb"==== /klog: the kernel log \((\d+) bytes\) ====\n").match(everything, at + len(mark) + len(lines))
    if at < 0 or not found:
        fail("/all: no gpu part and then the kernel log")
    size = int(found.group(1))
    log = everything[found.end():found.end() + size]
    if log[:len(klog)] != klog:
        fail("/all: the kernel log's part is another text")
    end = b"==== /boot.log: none on a mounted volume ====\n==== /boot.previous.log: none on a mounted volume ====\n==== end ====\n"
    if everything[found.end() + size:] not in (end, b"\n" + end):
        fail("/all without volumes ends with %r" % everything[found.end() + size:][:300])
    status, headers, body = get(port, "/boot.log")
    if status != "HTTP/1.1 404 Not Found" or not body.startswith(b"no boot.log on a mounted volume"):
        fail("/boot.log without a volume: %r %r" % (status, body))
    status, headers, body = get(port, "/klog", "HEAD")
    if status != "HTTP/1.1 200 OK" or body != b"" or int(headers.get("content-length", "0")) < len(klog):
        fail("HEAD /klog: %r %r %d bytes" % (status, headers, len(body)))

# A request for /gpu whose head is `total` bytes.
def head_of(total, end=b"\r\n\r\n"):
    start = b"GET /gpu HTTP/1.1\r\nHost: brix\r\nX-Fill: "
    return start + b"a" * (total - len(start) - len(end)) + end

def refusals(port):
    for path in ("/nothing", "/klog/", "/KLOG", "/devices/klog", "/../devices/klog", "/system/settings/shadow",
                 "//", "/all/../klog", "/%6blog", "/favicon.ico", "/\x1b[2Jx", "/\x1b]0;x\x07", "/klog%00", "/klog\x00",
                 "/klog\x00/../devices/klog", "//klog", "/./klog", "/klog/.", "/%2e%2e/devices/klog", "/..%2fdevices%2fklog", "/klog;x"):
        status, headers, body = get(port, path)
        if status != "HTTP/1.1 404 Not Found" or body != NO_PAGE:
            fail("%r: %r %r" % (path, status, body[:100]))
    # What follows a ? or a # is no part of the path, whatever it names;
    # nor does a header say where to look.
    gpu = page(port, "/gpu")
    for path in ("/gpu?/devices/klog", "/gpu#/devices/klog", "/gpu?../../devices/klog#/system/settings/shadow"):
        if page(port, path) != gpu:
            fail("%r is not the /gpu page" % path)
    status, headers, body = split(exchange(port, b"GET /gpu HTTP/1.1\r\nHost: ../../devices/klog\r\nX-Forwarded-Host: /devices/klog\x00\r\n\r\n"))
    if status != "HTTP/1.1 200 OK" or body != gpu:
        fail("with a Host of chance: %r" % status)
    # The limit at its edge: a head of 2047 and of 2048 bytes is read (a
    # body after it too), one of 2049 is not.
    for total, more in ((2047, b""), (2048, b""), (2048, b"x=1&y=2")):
        status, headers, body = split(exchange(port, head_of(total) + more))
        if status != "HTTP/1.1 200 OK" or body != gpu:
            fail("a head of %d bytes (and %d after it): %r" % (total, len(more), status))
    for total, end in ((2049, b"\r\n\r\n"), (2049, b"\n\n"), (2048, b"")):
        status, headers, body = split(exchange(port, head_of(total, end), True))
        if status != "HTTP/1.1 431 Request Header Fields Too Large":
            fail("a head of %d bytes ending %r: %r" % (total, end, status))
    # A head that comes a byte at a time.
    s = socket.create_connection((HOST, port), timeout=60)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    for byte in b"GET /gpu HTTP/1.1\r\nHost: brix\r\n\r\n":
        s.sendall(bytes([byte]))
        time.sleep(0.01)
    status, headers, body = split(rest(s))
    if status != "HTTP/1.1 200 OK" or body != gpu:
        fail("a head sent a byte at a time: %r" % status)
    for method in ("POST", "PUT", "DELETE", "PATCH"):
        status, headers, body = split(exchange(port, ("%s /klog HTTP/1.1\r\nHost: brix\r\nContent-Length: 7\r\n\r\nx=1&y=2" % method).encode(), True))
        if status != "HTTP/1.1 405 Method Not Allowed" or headers.get("allow") != "GET, HEAD" or not body.startswith(b"logserve answers GET and HEAD only"):
            fail("%s: %r %r" % (method, status, headers))
    status, headers, body = split(exchange(port, b"OPTIONS * HTTP/1.1\r\nHost: brix\r\n\r\n", True))
    if status != "HTTP/1.1 405 Method Not Allowed":
        fail("OPTIONS: %r" % status)
    for what, raw in (("headers of 6000 bytes", b"GET /klog HTTP/1.1\r\nHost: brix\r\nX-Long: " + b"a" * 6000 + b"\r\n\r\n"),
                      ("a request line of 3000 bytes", b"GET /" + b"k" * 3000 + b" HTTP/1.1\r\nHost: brix\r\n\r\n")):
        status, headers, body = split(exchange(port, raw, True))
        if status != "HTTP/1.1 431 Request Header Fields Too Large":
            fail("%s: %r" % (what, status))
    for what, raw in (("a TLS greeting", b"\x16\x03\x01\x02\x00\x01\x00\x01\xfc\x03\x03"), ("no HTTP version", b"GET /klog\r\n\r\n"),
                      ("a proxy's target", b"GET http://brix/klog HTTP/1.1\r\nHost: brix\r\n\r\n"), ("HTTP/2's preface", b"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n")):
        status, headers, body = split(exchange(port, raw, True))
        if status != "HTTP/1.1 400 Bad Request":
            fail("%s: %r" % (what, status))

def patience(port):
    slow = socket.create_connection((HOST, port), timeout=60)
    slow.sendall(b"GET /klog HTTP/1.1\r\nHost: brix\r\nX-Never: ")
    silent = socket.create_connection((HOST, port), timeout=60)
    started = time.time()
    status, headers, body = get(port, "/system")
    # (Answered while the two are still there: not once logserve has
    # given them up.)
    if status != "HTTP/1.1 200 OK" or select.select([slow, silent], [], [], 0)[0]:
        fail("behind a request that never ends the next client got %r after %.1f s" % (status, time.time() - started))
    data = rest(slow)
    took = time.time() - started
    if not data.startswith(b"HTTP/1.1 408 Request Timeout\r\n") or took < 3.5 or took > 30:
        fail("the request that never ends: %r after %.1f s" % (data[:60], took))
    data = rest(silent)
    took = time.time() - started
    if data != b"" or took > 30:
        fail("the connection that says nothing: %r after %.1f s" % (data[:60], took))

def together(port, out):
    klog = open(out + "/klog", "rb").read()
    results = [None] * 3
    def one(k):
        results[k] = get(port, "/klog")
    threads = [threading.Thread(target=one, args=(k,)) for k in range(3)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    for status, headers, body in results:
        if status != "HTTP/1.1 200 OK" or body[:len(klog)] != klog or headers.get("content-length") != str(len(body)):
            fail("one of three at once: %r, %d bytes" % (status, len(body)))

def closed(port):
    try:
        data = exchange(port, b"GET / HTTP/1.0\r\n\r\n", timeout=10)
    except (ConnectionRefusedError, ConnectionResetError, socket.timeout):
        data = b""
    if data:
        fail("the port still answers: %r" % data[:60])

# A request's answer within `timeout` seconds, or b"" (the kernel had no
# socket for the connection: nothing comes).
def answer(port, raw, timeout):
    s = socket.create_connection((HOST, port), timeout=timeout)
    data = b""
    try:
        s.sendall(raw)
        while True:
            got = s.recv(65536)
            if not got:
                break
            data += got
    except (socket.timeout, ConnectionResetError):
        data = b""
    return s, data

# Clients that get their answer and keep their connection open, one after
# the other: `count` of them, or (0) as many as are answered, which is as
# many as the kernel has sockets for. How many there are is written to
# `out`; then they are kept until this is ended.
def hold(port, count, out):
    kept = []
    refused = 0
    while len(kept) < (count or 64) and refused < 2:
        s, data = answer(port, b"GET /gpu HTTP/1.1\r\nHost: brix\r\n\r\n", 2)
        if data.startswith(b"HTTP/1.1 200 OK\r\n"):
            kept.append(s)
            refused = 0
            continue
        # (Not kept: nothing of it is to come later.)
        s.close()
        if count:
            fail("a client that keeps its connection got no answer")
        # (A connection that ended a moment ago keeps its socket for two
        # seconds more: once that is over, one more try says whether the
        # kept ones have them all.)
        refused += 1
        if refused < 2:
            time.sleep(2.5)
    open(out + ".part", "w").write("%d\n" % len(kept))
    os.rename(out + ".part", out)
    time.sleep(300)

# A request is answered within `seconds`, tried again every second.
def later(port, seconds):
    started = time.time()
    while True:
        s, data = answer(port, b"GET /system HTTP/1.1\r\nHost: brix\r\n\r\n", 3)
        s.close()
        if data.startswith(b"HTTP/1.1 200 OK\r\n"):
            break
        if time.time() - started > seconds:
            fail("no request was answered for %d s" % seconds)
        time.sleep(1)
    print("%d" % (time.time() - started))

def free_memory(port):
    return int(re.search(r"^memory: \d+ MiB, (\d+) free$", page(port, "/system").decode(), re.M).group(1))

# As many requests one after the other as the machine's free memory has
# rings for the kernel's sockets (a connection's two rings are 32 KiB: 32
# connections a MiB), and some more: each is answered, and the machine has
# the memory it had (within 3 MiB). Each client closes its side once it
# has asked, as nc does: the kernel's socket is free again at once.
def many(port):
    free = free_memory(port)
    count = 32 * free + 64
    for k in range(count):
        s = socket.create_connection((HOST, port), timeout=30)
        s.sendall(b"GET /system HTTP/1.0\r\n\r\n")
        s.shutdown(socket.SHUT_WR)
        try:
            data = rest(s)
        except socket.timeout:
            data = b""
        if not data.startswith(b"HTTP/1.1 200 OK\r\n") or b"\r\n\r\nmemory: " not in data:
            fail("the %d. request of %d one after the other: %r" % (k + 1, count, data[:60]))
    after = free_memory(port)
    if free - after > 3:
        fail("%d requests took %d MiB of the machine's memory for good (%d MiB free before, %d after)" % (count, free - after, free, after))
    print(count)

# The two volumes' logs as the check wrote them (seq 200000, seq 300000
# 500000), a megabyte of each.
def previous(volume):
    return (numbers(1, 200000) if volume == 0 else numbers(300000, 500000))[:MIB]

# `device` virtio: the boot the logs were saved at (disk0's boot.log is
# the kernel log's beginning); igb: the boot after (it is another text).
def volumes(port, out, device="virtio"):
    klog = page(port, "/klog")
    log = page(port, "/boot.log")
    if device == "virtio":
        if b"brixOS: probing network\n" not in log or klog[:len(log)] != log:
            fail("/boot.log is not the kernel log as it was saved (%d bytes)" % len(log))
        open(out + "/boot.log", "wb").write(log)
        named = b"==== /boot.log: /volumes/disk0/boot.log (%d bytes: the kernel log's first bytes above, not repeated) ====\n==== /boot.previous.log: " % len(log)
    else:
        if log != open(out + "/boot.log", "rb").read() or klog[:len(log)] == log:
            fail("/boot.log is not the log saved at the boot before (%d bytes)" % len(log))
        named = b"==== /boot.log: /volumes/disk0/boot.log (%d bytes) ====\n" % len(log) + log + b"==== /boot.previous.log: "
    both = page(port, "/boot.previous.log")
    part = [b"==== /volumes/disk%d/boot.previous.log ====\n" % v + previous(v) for v in (0, 1)]
    if both not in (part[0] + b"\n" + part[1], part[1] + b"\n" + part[0]):
        fail("/boot.previous.log of two volumes: %d bytes, beginning %r" % (len(both), both[:80]))
    # A client that closes its side once it has asked (as nc does) gets
    # the two megabytes all the same.
    s = socket.create_connection((HOST, port), timeout=60)
    s.sendall(b"GET /boot.previous.log HTTP/1.0\r\n\r\n")
    s.shutdown(socket.SHUT_WR)
    status, headers, body = split(rest(s))
    if status != "HTTP/1.1 200 OK" or body != both:
        fail("to a client that closed its side after the request: %r, %d bytes of %d" % (status, len(body), len(both)))
    # The memory an answer took is given back: after twelve more of them
    # the machine has what it had (within 8 MiB).
    free = []
    for turn in (0, 1):
        free.append(int(re.search(r"^memory: \d+ MiB, (\d+) free$", page(port, "/system").decode(), re.M).group(1)))
        for k in range(12 if turn == 0 else 0):
            if page(port, "/boot.previous.log") != both:
                fail("/boot.previous.log is another text the %d. time" % (k + 2))
    if free[0] - free[1] > 8:
        fail("twelve answers of two megabytes took %d MiB of the machine's memory for good" % (free[0] - free[1]))
    everything = page(port, "/all")
    if (b"\nnet0: %s, hardware address" % device.encode()) not in everything:
        fail("/all: no %s link" % device)
    if named not in everything:
        fail("/all: not the boot.log (named and not repeated where it is the kernel log's beginning, whole where it is another boot's)")
    for v in (0, 1):
        if (b"\n==== /boot.previous.log: /volumes/disk%d/boot.previous.log (%d bytes: its first megabyte) ====\n" % (v, MIB) + previous(v) + b"\n====") not in everything:
            fail("/all: not the volume disk%d's boot.previous.log" % v)
    if not everything.endswith(b"\n==== end ====\n") or everything.count(log) != 1:
        fail("/all with volumes: the end, or the kernel log twice")

def beside(port):
    started = time.time()
    status, headers, body = get(port, "/system")
    # (While that client is still silent, 14 s: not once it reads again,
    # nor once it was given up.)
    if status != "HTTP/1.1 200 OK" or time.time() - started > 9:
        fail("beside a client that does not read, the next got %r after %.1f s" % (status, time.time() - started))

def listen(port, out):
    listener = socket.socket()
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind((HOST, port))
    listener.listen(1)
    open(out + ".ready", "w").close()
    listener.settimeout(120)
    s, _ = listener.accept()
    s.settimeout(120)
    # (It has nothing to say and closes its own side at once, as a
    # listener without input does, nc -l PORT < /dev/null: the text
    # comes all the same.)
    s.shutdown(socket.SHUT_WR)
    data = rest(s)
    open(out + ".part", "wb").write(data)
    os.rename(out + ".part", out)

def sent(out, device="virtio"):
    data = open(out, "rb").read()
    if not data.startswith(b"==== /system: the machine and its network links ====\nmemory: ") or (b"\nnet0: %s, hardware address" % device.encode()) not in data:
        fail("what was sent does not begin with the machine and its links")
    if b"\n==== /klog: the kernel log (" not in data or b"\nbrixOS: probing network\n" not in data or not data.endswith(b"\n==== end ====\n"):
        fail("what was sent lacks the kernel log or its end")
    for v in (0, 1):
        if previous(v) not in data:
            fail("what was sent lacks the volume disk%d's boot.previous.log" % v)
    print(len(data))

# Eight clients at once ask three times for everything, on a machine that
# has no memory for that many megabytes: each gets the whole text or the
# word that there is no memory for it, and logserve goes on.
def crowd(port):
    results = []
    def one():
        for turn in range(3):
            try:
                results.append(get(port, "/all"))
            except OSError as error:
                results.append((repr(error), {}, b""))
    threads = [threading.Thread(target=one) for k in range(8)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    whole = refused = 0
    for status, headers, body in results:
        if status == "HTTP/1.1 200 OK" and headers.get("content-length") == str(len(body)) and body.endswith(b"\n==== end ====\n"):
            whole += 1
        elif status == "HTTP/1.1 503 Service Unavailable" and body == b"no memory for this answer at the moment\n":
            refused += 1
        else:
            fail("one of eight at once on a machine with little memory: %r, %d bytes" % (status, len(body)))
    try:
        status, headers, body = get(port, "/system")
    except OSError as error:
        status = repr(error)
    if status != "HTTP/1.1 200 OK":
        fail("after eight clients at once on a machine with little memory logserve is gone: %r" % status)
    print("%d %d" % (whole, refused))

def igb(port, out):
    system = page(port, "/system").decode()
    for pattern in (r"^net0: igb, hardware address 52:54:00:12:34:56, cable connected, 1000 Mbit/s$",
                    r"^net0: address 10\.0\.2\.15, netmask 255\.255\.255\.0, gateway 10\.0\.2\.2, name server 10\.0\.2\.3 \(by DHCP\)$"):
        if not re.search(pattern, system, re.M):
            fail("/system on igb lacks %s:\n%s" % (pattern, system))
    klog = page(port, "/klog")
    if not re.search(rb"^brixOS: /devices/net0, [0-9a-f:]* \(igb, message-signalled\)$", klog, re.M):
        fail("/klog on igb lacks the device's line")
    volumes(port, out, "igb")

command = sys.argv[1]
if command == "pages": pages(int(sys.argv[2]), sys.argv[3])
elif command == "refusals": refusals(int(sys.argv[2]))
elif command == "patience": patience(int(sys.argv[2]))
elif command == "together": together(int(sys.argv[2]), sys.argv[3])
elif command == "closed": closed(int(sys.argv[2]))
elif command == "hold": hold(int(sys.argv[2]), int(sys.argv[3]), sys.argv[4])
elif command == "later": later(int(sys.argv[2]), int(sys.argv[3]))
elif command == "many": many(int(sys.argv[2]))
elif command == "free": print(free_memory(int(sys.argv[2])))
elif command == "crowd": crowd(int(sys.argv[2]))
elif command == "volumes": volumes(int(sys.argv[2]), sys.argv[3])
elif command == "beside": beside(int(sys.argv[2]))
elif command == "listen": listen(int(sys.argv[2]), sys.argv[3])
elif command == "sent": sent(sys.argv[2])
elif command == "igb": igb(int(sys.argv[2]), sys.argv[3])
else: fail("no command " + command)
PY
host() { python3 "$work/client.py" "$@" || fail "the host's side: $*"; }
# Clients on the host that got their answer and keep their connections
# open ($1 of them; 0: as many as are answered), until release.
hold() {
    rm -f "$work/held"
    python3 "$work/client.py" hold "$port" "$1" "$work/held" &
    holder_pid=$!
    for i in $(seq 1 600); do
        [[ -e "$work/held" ]] && break
        kill -0 "$holder_pid" 2> /dev/null || fail "the host's clients that keep their connections"
        sleep 0.1
    done
    [[ -e "$work/held" ]] || fail "the host's clients that keep their connections did not get their answers"
}
release() {
    kill "$holder_pid" 2> /dev/null || true
    wait "$holder_pid" 2> /dev/null || true
    holder_pid=
}
free_port() { python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()'; }
port=$(free_port)
second=$(free_port)
listening=$(free_port)
nobody=$(free_port)

# Two disks of 4 MiB for the volumes.
head -c 4194304 /dev/zero > "$work/disk0.img"
head -c 4194304 /dev/zero > "$work/disk1.img"

# brixOS in QEMU with the disks and the network the arguments give, typed
# at through a FIFO; the serial line into serial.txt.
boot() {
    rm -f "$work/input"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 -m "$memory" -display none -no-reboot -cpu max -serial stdio -kernel "$work/brix.img" \
        -drive "file=$work/disk0.img,format=raw,if=virtio" -drive "file=$work/disk1.img,format=raw,if=virtio" \
        "$@" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    prompts=0
}
stop() {
    exec 3>&-
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
}
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 600 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
prompts=0
# The machine's memory (one of the last boots has little).
memory=256M
type_line() {
    prompts=$((prompts + 1))
    wait_for '^root@brix:[^#]*# $' "$prompts"
    printf '%s\r' "$1" >&3
}
# Ctrl+C to the program in front, once it has said $1.
interrupt_after() {
    wait_for "$1" "${2:-1}"
    sleep 0.3
    printf '\003' >&3
}
forwards="hostfwd=tcp:127.0.0.1:$port-:8080,hostfwd=tcp:127.0.0.1:$second-:8081"

boot -monitor none -netdev "user,id=n0,$forwards" -device virtio-net-pci,netdev=n0
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
type_line 'logserve'
wait_for '^logserve: the kernel log and this machine.s state, for another computer of this network$'
wait_for '^  open http://10\.0\.2\.15:8080/all on another computer of this network (a browser, or curl)$'
wait_for '^  while this runs, everyone on the network can read the log$'
wait_for '^  Ctrl+C ends it$'
echo "ok   started at the terminal, logserve says the address to open, that the network can read the log while it runs, and how it ends"
host pages "$port" "$work"
# (What the machine has free of its 256 MiB: the machine with little
# memory further down is made from it.)
free=$(host free "$port")
wait_for '^10\.0\.2\.2 GET /: 200, [0-9]* bytes$'
wait_for '^10\.0\.2\.2 GET /klog: 200, [0-9]* bytes$' 2
wait_for '^10\.0\.2\.2 GET /gpu: 200, [0-9]* bytes$'
wait_for '^10\.0\.2\.2 GET /system: 200, [0-9]* bytes$'
wait_for '^10\.0\.2\.2 GET /all: 200, [0-9]* bytes$'
wait_for '^10\.0\.2\.2 GET /boot\.log: 404, nothing of it on this machine$'
wait_for '^10\.0\.2\.2 HEAD /klog: 200, the head alone$'
echo "ok   from the host: / lists the pages, /klog is the kernel log with the boot's lines, /gpu says that no line begins with gpu:, /system the machine and its link, /all every part under its heading; a HEAD; no boot.log without a volume (404); a line a request on the terminal"
host refusals "$port"
wait_for '^10\.0\.2\.2 GET /system/settings/shadow: 404, no such page$'
wait_for '^10\.0\.2\.2 GET /?\[2Jx: 404, no such page$'
wait_for '^10\.0\.2\.2 GET /?]0;x?: 404, no such page$'
wait_for '^10\.0\.2\.2 GET /klog?/\.\./devices/klog: 404, no such page$'
wait_for '^10\.0\.2\.2 POST /klog: 405, only GET and HEAD are answered$'
wait_for '^10\.0\.2\.2: 431, a request.s head longer than 2048 bytes$' 5
wait_for '^10\.0\.2\.2: 400, not an HTTP/1 request$' 4
# (Nothing a client sent is on the terminal as it came: no escape that
# clears the screen or names the window, no bell, no NUL.)
python3 - "$work/serial.txt" <<'PY' || fail "a request's control characters reached the terminal"
import sys
data = open(sys.argv[1], "rb").read()
sys.exit(1 if b"\x1b[2J" in data or b"\x1b]" in data or b"\x07" in data or b"\x00" in data else 0)
PY
echo "ok   only the fixed pages: every other path is 404 (a file's path, .., %2e, a NUL, // too; a query or a fragment is no part of the path), POST, PUT, DELETE, PATCH and OPTIONS are refused (405), a head of 2049 bytes (431; one of 2048 is read, and one that comes a byte at a time), what is no HTTP/1 request (400); a path's control characters do not reach the terminal"
host patience "$port"
wait_for '^10\.0\.2\.2: 408, the request did not end within 5 s$'
host together "$port" "$work"
wait_for '^10\.0\.2\.2 GET /klog: 200, [0-9]* bytes$' 5
echo "ok   behind a request that never ends and a connection that says nothing the next client is answered at once; both are closed after the time limit (408); three clients at once get the whole log each"
# (A client that got its answer and keeps its connection open, as a
# browser may: it is still there when logserve is started again.)
hold 1
interrupt_after '^10\.0\.2\.2 GET /klog: 200, [0-9]* bytes$' 5
wait_for '^logserve: ended ([0-9]* requests answered)$'
type_line 'echo status $?'
wait_for '^status 0$'
host closed "$port"
echo "ok   Ctrl+C ends logserve with 0, and the port is closed"

# Started again at once: the port is its own again, though the kernel
# still has the connection of the client that keeps its side open (and
# those of the last answers, which take their leave for two seconds).
type_line 'logserve'
wait_for '^  open http://10\.0\.2\.15:8080/all on another computer of this network (a browser, or curl)$' 2
wait_for '^  Ctrl+C ends it$' 2
[[ "$(host later "$port" 10)" =~ ^[0-9]+$ ]] || fail "started again, logserve does not answer"
release
echo "ok   started again at once on the same port, logserve listens and answers (a client of the run before still keeps its connection)"
# Clients that keep their connection after the answer take a socket of
# the kernel each, as long as it lets them: as many as it has sockets for
# are answered, then none is; twenty seconds after logserve closed them
# the kernel gives them up, and the next request is answered while they
# still hold on.
hold 0
kept=$(cat "$work/held")
(( kept >= 1 )) || fail "no client that keeps its connection was answered"
waited=$(host later "$port" 45)
[[ "$waited" =~ ^[0-9]+$ ]] || fail "with $kept connections kept open by their clients, no request is answered any more"
release
echo "ok   $kept clients that keep their connection open after the answer are answered, then the kernel has no socket left (it has 32); it gives the kept ones up, and a request is answered again after $waited s while they hold on"
interrupt_after '^10\.0\.2\.2 GET /system: 200, [0-9]* bytes$' 5
wait_for '^logserve: ended ([0-9]* requests answered)$' 2

# Two volumes with saved logs: disk0's boot.log is the kernel log as it
# is now, both have a boot.previous.log of more than a megabyte; disk1's
# boot.log is a link to another file (not a saved log: it is not given).
type_line 'brixfs format /devices/disk0 && brixfs format /devices/disk1 && cat /devices/klog > /volumes/disk0/boot.log && seq 200000 > /volumes/disk0/boot.previous.log && seq 300000 500000 > /volumes/disk1/boot.previous.log && ln -s /volumes/disk1/boot.previous.log /volumes/disk1/boot.log && sync; echo volumes $?'
wait_for '^volumes 0$'
# (A client in brixOS that asks for the two megabytes and reads nothing
# for 14 s is started first, behind the shell: it connects once
# logserve listens.)
type_line 'reader 10.0.2.15 8081 /boot.previous.log 14 &'
type_line 'logserve 8081'
wait_for '^  open http://10\.0\.2\.15:8081/all on another computer of this network (a browser, or curl)$'
wait_for '^reader: asked for /boot.previous.log, reading nothing for 14 s$'
host beside "$second"
(( $(output | grep -c 'cut off after [0-9]* of [0-9]* bytes (the client took nothing' || true) == 0 )) || fail "the client beside the one that does not read was answered only when that one had been given up"
host volumes "$second" "$work"
wait_for '^10\.0\.2\.2 GET /boot\.previous\.log: 200, [0-9]* bytes$' 14
echo "ok   on the port given, with two volumes: /boot.log is the saved log (a link of that name on the other volume is not followed), /boot.previous.log both volumes' under their names (a megabyte of each, whole; also to a client that closed its side after asking, and twelve times over without the memory going), /all has them and names the boot.log that is the kernel log's beginning without repeating it"
wait_for '^10\.0\.2\.15 GET /boot\.previous\.log: 200, cut off after [0-9]* of [0-9]* bytes (the client took nothing for 10 s)$'
wait_for '^reader: [0-9]\{4,6\} bytes came, then the connection.s end$'
echo "ok   a client that stops reading a long answer is given up after 10 s (it finds the answer cut short); the others are answered meanwhile"
interrupt_after '^reader: [0-9]* bytes came'
wait_for '^logserve: ended ([0-9]* requests answered)$' 3

# The other way: logserve connects out and writes the /all text.
python3 "$work/client.py" listen "$listening" "$work/sent.txt" &
listener_pid=$!
for i in $(seq 1 100); do
    [[ -e "$work/sent.txt.ready" ]] && break
    sleep 0.1
done
type_line "logserve send 10.0.2.2 $listening; echo sent \$?"
wait_for '^sent 0$'
for i in $(seq 1 300); do
    [[ -e "$work/sent.txt" ]] && break
    sleep 0.1
done
[[ -e "$work/sent.txt" ]] || fail "the host's listener did not get the text to its end"
wait "$listener_pid" 2> /dev/null || true
listener_pid=
size=$(python3 "$work/client.py" sent "$work/sent.txt") || fail "what logserve send wrote to the host's listener"
wait_for "^logserve: sent $size bytes to 10\.0\.2\.2:$listening\$"
type_line "logserve send 10.0.2.2 $nobody; echo refused \$?"
wait_for "^logserve: cannot connect to 10\.0\.2\.2:$nobody (nobody listens there, or it cannot be reached)\$"
wait_for '^refused 1$'
type_line 'logserve send nowhere; echo usage $?'
wait_for '^usage: logserve \[PORT\] '
wait_for '^usage 1$'
echo "ok   logserve send HOST PORT writes the /all text ($size bytes) to a listener on the host (one that closed its own side at once) and ends with 0; where nobody listens it says so (1); wrong arguments: the usage (1)"

# Many requests one after the other: more connections than the machine's
# free memory has room for their rings (the kernel must use a socket's
# memory again), every one answered, and logserve no bigger for them (a
# request's records go when its turn of the loop is over).
type_line 'logserve'
wait_for '^  Ctrl+C ends it$' 4
count=$(host many "$port")
[[ "$count" =~ ^[0-9]+$ ]] || fail "many requests one after the other"
printf '\003' >&3
wait_for '^logserve: ended ([0-9]* requests answered)$' 4
echo "ok   $count requests one after the other (more connections than the free memory has rings for) are answered every one, and the machine has the memory it had"
stop

# An igb card (the machine's kind) whose cable is out when brixOS boots:
# no address, said at the start; once the cable is in and DHCP has
# answered, logserve says the address, and the pages come through the
# card (the volumes are mounted from the disks).
monitor() {
    python3 - "$work/monitor" "$1" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sys.argv[1])
s.settimeout(0.5)
def drain():
    try:
        while s.recv(4096): pass
    except socket.timeout:
        pass
drain()
s.sendall(sys.argv[2].encode() + b"\n")
time.sleep(0.15)
drain()
PY
}
boot -machine q35 -S -monitor "unix:$work/monitor,server,nowait" -netdev "user,id=n0,$forwards" -device igb,netdev=n0
for i in $(seq 1 100); do
    [[ -S "$work/monitor" ]] && break
    sleep 0.1
done
monitor 'set_link n0 off'
monitor 'cont'
wait_for '^brixOS: net0 has no address yet (DHCP is asking)$'
type_line 'logserve'
wait_for '^  net0 has no address yet (DHCP is asking): it is said here when it has one$'
wait_for '^  Ctrl+C ends it$'
# (QEMU's igb says its link is up once the driver has reset it, though
# no frame passes: pulled again now, its status register says so, and
# logserve at its next look at the link.)
monitor 'set_link n0 off'
wait_for '^logserve: net0 has no cable: nothing reaches this machine over it until it has$'
echo "ok   igb with the cable out: logserve says that net0 has no address yet, then that it has no cable, and listens all the same"
monitor 'set_link n0 on'
wait_for '^logserve: net0 has its cable again$'
wait_for '^logserve: open http://10\.0\.2\.15:8080/all on another computer of this network (a browser, or curl)$'
host igb "$port" "$work"
interrupt_after '^10\.0\.2\.2 GET /all: 200, [0-9]* bytes$'
wait_for '^logserve: ended ([0-9]* requests answered)$'
echo "ok   the cable in: logserve says so, and the address when DHCP has given one; over igb the pages, the megabyte logs of both volumes and /all come whole (the boot.log of the boot before in it, as another text than this boot's log); Ctrl+C ends it"

# A machine with little memory (10 MiB free of it when it has booted) and
# four megabyte logs on its volumes (the boot.log of each made one now):
# everything is more megabytes than the machine has, and eight clients at
# once ask for it. Each gets the word that there is no memory for the
# answer at the moment (503), or the text whole; logserve is not ended
# at a page the kernel has no memory behind, and answers on.
type_line 'rm /volumes/disk1/boot.log; seq 1 200000 > /volumes/disk0/boot.log && seq 1 200000 > /volumes/disk1/boot.log && sync; echo more $?'
wait_for '^more 0$'
stop
memory=$((256 - free + 10))M
boot -monitor none -netdev "user,id=n0,$forwards" -device virtio-net-pci,netdev=n0
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
type_line 'logserve'
wait_for '^  Ctrl+C ends it$'
crowd=$(host crowd "$port")
[[ "$crowd" =~ ^[0-9]+\ [0-9]+$ ]] || fail "eight clients at once on a machine with little memory"
(( $(output | grep -c 'ended by signal' || true) == 0 )) || fail "logserve was ended on a machine with little memory"
printf '\003' >&3
wait_for '^logserve: ended ([0-9]* requests answered)$'
echo "ok   on a machine with little memory ($memory, 10 MiB of it free) eight clients at once ask for /all with four megabyte logs: ${crowd#* } are told that there is no memory for the answer (503), ${crowd% *} get it whole, and logserve goes on"
stop
memory=256M

# A machine without a network device: said, and 1.
boot -monitor none -nic none
type_line 'logserve; echo status $?'
wait_for '^logserve: this machine has no network (no network device was found at boot): there is nothing to answer on$'
wait_for '^status 1$'
type_line 'logserve send 10.0.2.2 9; echo sending $?'
wait_for '^logserve: this machine has no network (no network device was found at boot)$'
wait_for '^sending 1$'
echo "ok   without a network device logserve says so and ends with 1"
echo PASS
