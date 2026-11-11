#!/usr/bin/env bash
# ping's Linux build (projects/ping with host.mlx: a datagram socket of
# IPPROTO_ICMP, poll, Ctrl+C read from a signal descriptor) on the
# loopback of a network namespace of its own, where ping sockets are
# allowed (most hosts forbid them: net.ipv4.ping_group_range): the
# forms of the count, the summary, the status; what is no name and what
# is no address; a reply from another address than the one asked (Linux
# answers a request to 0.0.0.0 from 127.0.0.1); a request the system
# does not take counted apart; Ctrl+C and a request to end, both with
# the summary; and that ping left running does not grow (it took some
# 1.8 KiB of the value arena a request: std.turns gives it back).
# brixOS's build has tools/check_brix_net.sh and check_brix_net_igb.sh.
#
#   tools/check_ping_linux.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
if ! command -v python3 > /dev/null; then
    echo "skip: no python3"
    exit 0
fi
if ! unshare -n true 2> /dev/null; then
    echo "skip: no network namespace of our own (unshare -n needs the right to make one)"
    exit 0
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"$compiler" --quiet projects/ping/main.mlx -o "$work/ping"
echo "ok   ping builds for Linux"
# (Status 3 from in there: skipped, and said why.)
status=0
unshare -n python3 - "$work/ping" <<'PY' || status=$?
import fcntl, os, re, signal, socket, struct, subprocess, sys, time

ping = sys.argv[1]

# The namespace's loopback up, ping sockets for every group.
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
flags = struct.unpack("16sH", fcntl.ioctl(s, 0x8913, struct.pack("16sH", b"lo", 0) + b"\0" * 14)[:18])[1]
fcntl.ioctl(s, 0x8914, struct.pack("16sH", b"lo", flags | 1) + b"\0" * 14)
try:
    open("/proc/sys/net/ipv4/ping_group_range", "w").write("0 2147483647")
except OSError:
    print("skip: ping sockets cannot be allowed here (net.ipv4.ping_group_range)")
    sys.exit(3)


def fail(what):
    print("FAIL " + what)
    sys.exit(1)


def run(*arguments):
    done = subprocess.run([ping] + list(arguments), capture_output=True, text=True, timeout=60)
    return done.returncode, done.stdout, done.stderr


def expect(arguments, status, out=(), err=()):
    code, said, complained = run(*arguments)
    if code != status:
        fail("ping %s: status %d, not %d\n%s%s" % (" ".join(arguments), code, status, said, complained))
    for text, lines in ((said, out), (complained, err)):
        for line in lines:
            if not re.search(line, text, re.M):
                fail("ping %s: no \"%s\" in\n%s" % (" ".join(arguments), line, text))


reply = r"^64 bytes from 127\.0\.0\.1: seq=%d time=[0-9.]+ ms$"
times = r"^round trip: least [0-9.]+ ms, mean [0-9.]+ ms, greatest [0-9.]+ ms$"
expect(["-c", "2", "127.0.0.1"], 0, out=[r"^PING 127\.0\.0\.1: 56 bytes of data$", reply % 1, reply % 2, r"^--- 2 sent, 2 received, 0% lost$", times])
expect(["127.0.0.1", "-c", "1"], 0, out=[r"^--- 1 sent, 1 received, 0% lost$"])
expect(["-c1", "127.0.0.1"], 0, out=[r"^--- 1 sent, 1 received, 0% lost$"])
expect(["127.0.0.1", "1"], 0, out=[r"^--- 1 sent, 1 received, 0% lost$"])
expect(["-c", "1", "localhost."], 0, out=[r"^PING localhost\. \(127\.0\.0\.1\): 56 bytes of data$"])
print("ok   the count before and after the host, joined to its option and as it was; the summary; localhost, also with the root's dot")
expect([], 1, err=[r"^usage: ping \[-c COUNT\] HOST$"])
expect(["-h"], 0, out=[r"^usage: ping \[-c COUNT\] HOST$"])
expect(["-c", "0", "127.0.0.1"], 1, err=[r"^ping: the count is a number from 1 on$"])
expect(["127.0.0.300"], 1, err=[r"^ping: 127\.0\.0\.300 is no address \(four numbers up to 255 with dots between\)$"])
expect(["a..b"], 1, err=[r"^ping: a\.\.b is no name \(parts of 1 to 63 characters with dots between, 253 in all\)$"])
expect(["x" * 300], 1, err=[r"^ping: x{60}\.\.\. is no name \(parts of 1 to 63 characters with dots between, 253 in all\)$"])
expect(["::1"], 1, err=[r"^ping: ::1 is an IPv6 address \(ping speaks IPv4\)$"])
print("ok   the usage, a count of 0, what is no address, what is no name (whole also of 300 characters), an IPv6 address")
# Linux answers a request to 0.0.0.0 from 127.0.0.1: another address
# than the one asked, and said so.
expect(["-c", "1", "0.0.0.0"], 0, out=[r"^64 bytes from 127\.0\.0\.1: seq=1 time=[0-9.]+ ms \(not the host asked\)$"])
# No way out of this namespace: the system does not take the request,
# which is then not one that was sent and lost.
expect(["-c", "1", "192.0.2.1"], 1, out=[r"^seq=1 could not be sent$", r"^--- 0 sent, 0 received, 1 could not be sent$"])
print("ok   a reply from another address than the one asked is said to be; a request the system does not take is counted apart")


def resident(pid):
    for line in open("/proc/%d/status" % pid):
        if line.startswith("VmRSS:"):
            return int(line.split()[1])
    return 0


# Left running: its memory after 3 and after 23 requests (it grew by some
# 36 KiB in those twenty seconds), then Ctrl+C: the summary, status 0.
running = subprocess.Popen([ping, "127.0.0.1"], stdout=subprocess.PIPE, text=True)
time.sleep(2.5)
before = resident(running.pid)
time.sleep(20)
after = resident(running.pid)
running.send_signal(signal.SIGINT)
said = running.communicate(timeout=10)[0]
if after - before > 12:
    fail("ping left running grew by %d KiB in twenty seconds (from %d)" % (after - before, before))
if running.returncode != 0 or not re.search(r"^--- (\d+) sent, \1 received, 0% lost$", said, re.M) or not re.search(times, said, re.M):
    fail("Ctrl+C after 22 s: status %d\n%s" % (running.returncode, "\n".join(said.splitlines()[-4:])))
print("ok   left running for twenty seconds ping does not grow (%d KiB, then %d); Ctrl+C ends it with the summary" % (before, after))
running = subprocess.Popen([ping, "127.0.0.1"], stdout=subprocess.PIPE, text=True)
time.sleep(1.5)
running.send_signal(signal.SIGTERM)
said = running.communicate(timeout=10)[0]
if running.returncode != 0 or not re.search(r"^--- ([1-9]) sent, \1 received, 0% lost$", said, re.M):
    fail("a request to end after 1.5 s: status %d\n%s" % (running.returncode, said))
print("ok   a request to end (SIGTERM) ends it with the summary too")
PY
[[ $status -eq 3 ]] && exit 0
[[ $status -eq 0 ]] || exit 1
echo PASS
