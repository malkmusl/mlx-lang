#!/usr/bin/env bash
# logpush (projects/brix/tools/logpush) in brixOS: the kernel log handed
# over by the machine itself. brixOS boots in QEMU with the init, the
# services klogsave and logpush, a brixfs volume and QEMU's user network
# (the guest is 10.0.2.15 and sees the host as 10.0.2.2), where a small
# server of this check plays GitHub's API (plain Python: it writes down
# every request, keeps the files it is sent, and answers as the API
# does). The images get their settings as the builder's do: brix-image
# --add puts a file in as settings/logpush (a test endpoint without TLS
# and a test's token made here: the only kind such an endpoint takes).
#
# - The service uploads by itself after the boot, once DHCP has answered:
#   it asks first whether the repository is there and private, then the
#   file decodes to the kernel log of that boot (its first line is the
#   kernel's first, its last a line of the log as `cat /devices/klog`
#   gives it later), under MACHINE/BOOT-DATE/klog-SECONDSs.txt (the
#   seconds since the boot, to the millisecond); the request has the
#   headers GitHub asks for; the service's log says it in one line,
#   naming the test endpoint and that it has no TLS. Its network is
#   brixOS's own socket calls: the kernel's log has no note of a Linux
#   call of logpush's.
# - The settings are root's alone: another user's program reads neither
#   them nor the accounts' passwords nor the disk's blocks, and reads the
#   kernel log.
# - `logpush` by hand in root's shell uploads the log as it is then
#   under another name of the same boot. By hand as another user
#   (tools/logpush-test/reader.mlx runs it so), who may not read the
#   settings, it asks the service, which uploads and whose lines are
#   said on the asker's terminal; asked again at once the service says
#   when it will; with the service stopped the user is told that nobody
#   is there to ask.
# - The service uploads once more after the settings' `again` seconds,
#   with what the kernel said since (a volume formatted meanwhile), and
#   stays for who asks.
# - A second boot uploads the log klogsave kept of the first as
#   previous-boot-klog.txt (the first boot's log as it was when klogsave
#   ran), once: by hand it is "there already". The kernel says nothing
#   more in that boot: the settings' 15 seconds later the service says
#   that the log has not grown, and uploads nothing.
# - A server that takes the kernel log and fails the log of the boot
#   before (500 twice, then 422): the service tries again for that log
#   alone, the kernel log stays the one file it uploaded; and its second
#   upload of the boot comes all the same, for the log of the boot
#   before, which now goes (the kernel log has not grown: not again).
# - The local settings file and what an editor leaves beside it
#   (logpush.local~, .logpush.local.swp, ...) are names git ignores: a
#   commit of everything takes none of them.
# - A server that answers 401: the reason in the log, on the terminal
#   and to an asker, no further try, and the token in nothing the
#   machine wrote (the serial line of every boot, the service's log, the
#   kernel log).
# - Over TLS (tools/logpush-test/tlsfront.mlx on the host puts std's own
#   TLS server in front of the check's, with a root and a certificate
#   made at its start; the image trusts that root: it is its
#   /system/settings/certificates), with a token that is no test's: the
#   uploads go through. With the roots the build's image has (GitHub's
#   authorities, projects/brix/settings/certificates) the same server is
#   not trusted: said, nothing sent. With the machine's clock at 2000
#   the certificate cannot be judged, with it at 2036 the certificate has
#   expired: each said with the date, nothing sent. On a processor
#   without a source of random numbers (QEMU's qemu64) the kernel's are
#   from the clock alone: no TLS key is made of them, nothing sent.
# - An image without settings: one line in the service's log, nothing
#   sent, the service done (not failed), by hand the same line. A machine
#   without a network device: said, also to who asks, nothing sent.
# - The boot is not slower: the time to the shell's prompt with the
#   service and without it, said.
# The kernel reads a graphics card's ROM from a Radeon alone, and QEMU
# has none: no /devices/gpurom here, so the ROM's upload is tests/487's
# (bytes of every value, asked for first, left alone when there).
#
#   tools/check_brix_logpush.sh [compiler]
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
server_pid=
front_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    [[ -n "$server_pid" ]] && kill "$server_pid" 2> /dev/null || true
    [[ -n "$front_pid" ]] && kill "$front_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
# (The serial line's end without the shell's line as it was typed, a
# letter at a time.)
fail() {
    echo "FAIL $1" >&2
    [[ -f "$work/serial.txt" ]] && tr '\r' '\n' < "$work/serial.txt" | grep -a -v '^root@brix:[^#]*# .' | grep -a . | tail -n 30 | cat -v >&2
    exit 1
}

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/services" "$work/plain-services" "$work/store"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet projects/brix/tools/brixfs/main.mlx -o "$work/brixfs-host"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/init/brixos.mlx -o "$work/impulse"
"$compiler" --quiet $brix projects/init/tool.mlx -o "$work/impulsectl"
"$compiler" --quiet $brix projects/brix/tools/logpush/main.mlx -o "$work/logpush"
"$compiler" --quiet $brix projects/brix/tools/klogsave/main.mlx -o "$work/klogsave"
"$compiler" --quiet $brix projects/brix/tools/brixfs/main.mlx -o "$work/brixfs"
"$compiler" --quiet $brix tools/logpush-test/reader.mlx -o "$work/reader"
"$compiler" --quiet tools/logpush-test/tlsfront.mlx -o "$work/tlsfront"
for program in cat echo seq sync; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
# The services as the image has them: the serial console's shell,
# klogsave and logpush (the repository's own files); and the same without
# logpush, to time the boot against.
cp projects/brix/settings/init/services/serial-shell.service projects/brix/settings/init/services/klogsave.service "$work/plain-services/"
cp "$work"/plain-services/*.service projects/brix/settings/init/services/logpush.service "$work/services/"
entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
plain_entries=()
for service in "$work"/plain-services/*.service; do plain_entries+=("settings/init/services/$(basename "$service")=$service"); done
# (The accounts' passwords too: root's alone to read like the settings.)
programs=("flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "logpush=$work/logpush" "klogsave=$work/klogsave" "brixfs=$work/brixfs" "reader=$work/reader" "settings/shadow=projects/brix/settings/shadow")
# The image as a build without settings makes it, and the one without the
# service.
"$work/brix-image" "$work/kernel" "$work/none.img" "${programs[@]}" "${entries[@]}" "$work/bin/"
"$work/brix-image" "$work/kernel" "$work/plain.img" "${programs[@]}" "${plain_entries[@]}" "$work/bin/"

# The builder's settings file holds a token: its name, the names an
# editor gives the copies it leaves beside a file and those of a copy a
# person keeps are no files of the repository (.gitignore). Asked of git
# itself, where this is a checkout.
if command -v git > /dev/null && git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
    for name in logpush.local 'logpush.local~' .logpush.local.swp .logpush.local.swo logpush.local.bak logpush.local.orig logpush.local.txt logpush.local2 logpush.local-old logpush.local_copy '#logpush.local#' '.#logpush.local' projects/brix/logpush.local 'projects/brix/logpush.local~'; do
        git check-ignore -q --no-index -- "$name" || fail "git does not ignore $name: a commit of everything (git add -A) would take the token with it"
    done
    echo "ok   git ignores the builder's settings file and what an editor leaves beside it (logpush.local, logpush.local~, .logpush.local.swp, logpush.local.bak, ...), in the top directory and misplaced below it: no commit takes the token"
else
    echo "ok   (not a git checkout: what git ignores is not asked here)"
fi

free_port() { python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()'; }
port=$(free_port)
tls_port=$(free_port)
# A test's token, made here: nothing of it is in the repository. And one
# that is no test's, for the way over TLS (no service's either: made up).
token="test-$(python3 -c 'import secrets; print(secrets.token_hex(20))')"
other_token="ghx_$(python3 -c 'import secrets; print(secrets.token_hex(24))')"
# The settings as a person writes them (CR LF from another system's
# editor in one), put into the image the way brix-image offers.
printf '# where this machine'"'"'s logs go\no/logs\n%s\nagain 15\ntest-endpoint http://10.0.2.2:%s\n' "$token" "$port" > "$work/settings"
printf 'o/denied\r\n%s\r\ntest-endpoint http://10.0.2.2:%s\r\n' "$token" "$port" > "$work/settings-denied"
printf 'o/shaky\n%s\nagain 10\ntest-endpoint http://10.0.2.2:%s\n' "$token" "$port" > "$work/settings-shaky"
"$work/brix-image" --add "$work/none.img" "$work/logs.img" "settings/logpush=$work/settings"
"$work/brix-image" --add "$work/none.img" "$work/denied.img" "settings/logpush=$work/settings-denied"
"$work/brix-image" --add "$work/none.img" "$work/shaky.img" "settings/logpush=$work/settings-shaky"
echo "ok   the kernel, the init, logpush, klogsave and the boot images build: one without settings, three with settings added by brix-image --add, one without the service"

# The host's side: GitHub's API as far as logpush uses it, and the looks
# at what it was sent. Every command ends with 1 and a line on a miss.
cat > "$work/host.py" <<'PY'
import base64, glob, json, os, re, sys, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

def fail(text):
    print("FAIL " + text, file=sys.stderr)
    sys.exit(1)

# GET /repos/OWNER/NAME gives the repository's record (o/logs is
# private); PUT /repos/OWNER/NAME/contents/PATH with {"message",
# "content"} makes the file (201; 422 when it is there, as GitHub answers
# a PUT without the file's sha); GET says whether it is there (200, 404).
# The repository o/denied takes no token (401). The repository o/shaky is
# as o/logs, but for the log of the boot before: its first two uploads
# fail on the server's side (500), its third is refused (422), the fourth
# is taken.
def serve(port, work, tokens):
    lock = threading.Lock()
    shaken = [0]

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *arguments):
            pass

        def answer(self, status, value):
            body = json.dumps(value).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def take(self, method):
            length = int(self.headers.get("Content-Length") or 0)
            body = self.rfile.read(length) if length else b""
            problems = []
            if self.headers.get("Authorization") not in ["Bearer " + token for token in tokens]:
                problems.append("Authorization is not Bearer and the token")
            if self.headers.get("User-Agent") != "brix-logpush":
                problems.append("User-Agent is %r" % self.headers.get("User-Agent"))
            if self.headers.get("Accept") != "application/vnd.github+json":
                problems.append("Accept is %r" % self.headers.get("Accept"))
            if self.headers.get("X-GitHub-Api-Version") != "2022-11-28":
                problems.append("X-GitHub-Api-Version is %r" % self.headers.get("X-GitHub-Api-Version"))
            match = re.fullmatch(r"/repos/([A-Za-z0-9._-]+/[A-Za-z0-9._-]+)/contents/([A-Za-z0-9._/-]+)", self.path)
            record = {"method": method, "path": self.path, "length": length}
            status, value = 404, {"message": "Not Found"}
            if self.path.startswith("/repos/o/denied"):
                status, value = 401, {"message": "Bad credentials", "documentation_url": "https://docs.github.com/rest", "status": "401"}
            elif self.path in ("/repos/o/logs", "/repos/o/shaky") and method == "GET":
                status, value = 200, {"id": 1, "full_name": self.path[7:], "owner": {"login": "o"}, "private": True, "visibility": "private"}
            elif match and match.group(1) in ("o/logs", "o/shaky"):
                stored = os.path.join(work, "store", match.group(2))
                if method == "GET":
                    if os.path.exists(stored):
                        status, value = 200, {"path": match.group(2)}
                else:
                    if self.headers.get("Content-Type") != "application/json":
                        problems.append("Content-Type is %r" % self.headers.get("Content-Type"))
                    try:
                        asked = json.loads(body)
                        content = base64.b64decode(asked["content"], validate=True)
                        record["message"] = asked["message"]
                        if set(asked) - {"message", "content", "branch"}:
                            problems.append("a body with %r" % sorted(asked))
                    except Exception as error:
                        problems.append("a body that is not the API's: %r" % error)
                        content = None
                    shaky = match.group(1) == "o/shaky" and match.group(2).endswith("/previous-boot-klog.txt")
                    if shaky:
                        with lock:
                            shaken[0] += 1
                    if content is None:
                        status, value = 422, {"message": "Invalid request."}
                    elif shaky and shaken[0] <= 2:
                        status, value = 500, {"message": "Server Error"}
                    elif shaky and shaken[0] == 3:
                        status, value = 422, {"message": "Invalid request."}
                    elif os.path.exists(stored):
                        status, value = 422, {"message": "Invalid request.\n\n\"sha\" wasn't supplied."}
                    else:
                        os.makedirs(os.path.dirname(stored), exist_ok=True)
                        with open(stored + ".part", "wb") as out:
                            out.write(content)
                        os.rename(stored + ".part", stored)
                        status, value = 201, {"content": {"path": match.group(2)}, "commit": {"message": asked["message"]}}
            else:
                problems.append("a request for %r" % self.path)
            record["status"] = status
            record["problems"] = problems
            with lock:
                with open(os.path.join(work, "requests.jsonl"), "a") as out:
                    out.write(json.dumps(record) + "\n")
            self.answer(status, value)

        def do_GET(self):
            self.take("GET")

        def do_PUT(self):
            self.take("PUT")

    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    open(os.path.join(work, "server.ready"), "w").close()
    server.serve_forever()

def requests(work):
    path = os.path.join(work, "requests.jsonl")
    if not os.path.exists(path):
        return []
    return [json.loads(line) for line in open(path)]

def stored(work, pattern):
    return sorted(glob.glob(os.path.join(work, "store", pattern)))

# Waits until `count` files of `pattern` are kept: their paths, a line each.
def wait(work, pattern, count, seconds):
    end = time.time() + seconds
    while time.time() < end:
        found = stored(work, pattern)
        if len(found) >= count:
            print("\n".join(os.path.relpath(f, os.path.join(work, "store")) for f in found))
            return
        time.sleep(0.2)
    fail("%d of %s uploaded within %d s, not %d: %r" % (len(stored(work, pattern)), pattern, seconds, count, requests(work)[-3:]))

# What the serial line holds between the lines BEGIN and END (the last
# such pair), its line ends as the machine's.
def between(serial, begin, end):
    text = open(serial, "rb").read().replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    start = text.rfind(b"\n" + begin + b"\n")
    stop = text.rfind(b"\n" + end + b"\n")
    if start < 0 or stop < start:
        fail("no %r ... %r on the serial line" % (begin, end))
    return text[start + len(begin) + 2:stop + 1]

# The kernel's first line on the serial line.
def first_kernel_line(serial):
    text = open(serial, "rb").read().replace(b"\r\n", b"\n")
    for line in text.split(b"\n"):
        if line.startswith(b"brixOS"):
            return line
    fail("no line of the kernel on the serial line")

# An uploaded kernel log against the log as `cat /devices/klog` printed
# it later (between KLOG-BEGIN and KLOG-END on the serial line): the
# upload is the beginning of it, its first line the kernel's first.
def klog(work, serial, name):
    upload = open(os.path.join(work, "store", name), "rb").read()
    later = between(serial, b"KLOG-BEGIN", b"KLOG-END")
    if not upload or not later.startswith(upload):
        fail("%s (%d bytes) is not the beginning of the kernel log as it was later (%d bytes)" % (name, len(upload), len(later)))
    lines = upload.split(b"\n")
    if upload[-1:] != b"\n" or lines[0] != first_kernel_line(serial):
        fail("%s does not begin with the kernel's first line, or ends inside a line: %r ... %r" % (name, lines[0], lines[-2:]))
    print("%d bytes, %d lines, from \"%s\" to \"%s\"" % (len(upload), len(lines) - 1, lines[0].decode("latin-1"), lines[-2].decode("latin-1")))

def main():
    command, work = sys.argv[1], sys.argv[2]
    if command == "serve":
        serve(int(sys.argv[3]), work, sys.argv[4:])
    elif command == "wait":
        wait(work, sys.argv[3], int(sys.argv[4]), int(sys.argv[5]))
    elif command == "klog":
        klog(work, sys.argv[3], sys.argv[4])
    elif command == "count":
        # How many requests there were (of a method, when one is named).
        print(len([r for r in requests(work) if len(sys.argv) < 4 or r["method"] == sys.argv[3]]))
    elif command == "problems":
        problems = [(r["method"], r["path"], r["problems"]) for r in requests(work) if r["problems"]]
        if problems:
            fail("requests that are not as GitHub's API asks: %r" % problems[:3])
        print(len(requests(work)))
    elif command == "names":
        # The kernel logs' names: one boot's directory, as many names.
        names = [os.path.relpath(f, os.path.join(work, "store")) for f in stored(work, sys.argv[3])]
        for name in names:
            if not re.fullmatch(r"brix/20[0-9]{2}-[01][0-9]-[0-3][0-9]T[0-2][0-9]-[0-5][0-9]-[0-5][0-9]Z/klog-[0-9]{7}\.[0-9]{3}s\.txt", name):
                fail("a kernel log named %r" % name)
        if len(set(os.path.dirname(name) for name in names)) != 1:
            fail("the uploads of one boot in more than one directory: %r" % names)
        print(len(names), os.path.dirname(names[0]))
    elif command == "grown":
        # The later upload holds the earlier one and more, `line` in it.
        early = open(os.path.join(work, "store", sys.argv[3]), "rb").read()
        late = open(os.path.join(work, "store", sys.argv[4]), "rb").read()
        if not late.startswith(early) or len(late) <= len(early) or not re.search(sys.argv[5].encode(), late[len(early):]):
            fail("%s is not %s and more, with %s in what came after" % (sys.argv[4], sys.argv[3], sys.argv[5]))
        print(len(late) - len(early))
    elif command == "previous":
        # The log of the boot before: the beginning of that boot's own
        # upload (klogsave wrote it early in that boot).
        kept = open(os.path.join(work, "store", sys.argv[3]), "rb").read()
        whole = open(os.path.join(work, "store", sys.argv[4]), "rb").read()
        if len(kept) < 200 or not whole.startswith(kept):
            fail("%s (%d bytes) is not the beginning of the boot before's log %s (%d bytes)" % (sys.argv[3], len(kept), sys.argv[4], len(whole)))
        print(len(kept))
    elif command == "part":
        # What stands on the serial line between the lines MARK-BEGIN
        # and MARK-END (the last such part).
        mark = sys.argv[3].encode()
        sys.stdout.buffer.write(between(work, mark + b"-BEGIN", mark + b"-END"))
    elif command == "long":
        # The uploaded log of the boot before is the first 256 KiB (what
        # logpush takes of a log) of the numbers 1 to 60000, a line each:
        # what the check wrote as that boot's log.
        kept = open(os.path.join(work, "store", sys.argv[3]), "rb").read()
        whole = "".join("%d\n" % number for number in range(1, 60001)).encode()
        if len(kept) != 262144 or kept != whole[:262144]:
            fail("%s (%d bytes) is not the first 262144 bytes of the long log" % (sys.argv[3], len(kept)))
        print(len(kept))
    elif command == "absent":
        # The token in nothing the machine wrote on its serial lines.
        token = sys.argv[3].encode()
        for name in sys.argv[4:]:
            text = open(name, "rb").read()
            if token in text or token[5:25] in text:
                fail("the token is on the serial line (%s)" % name)
        print(len(sys.argv) - 4)

main()
PY
host() { python3 "$work/host.py" "$@"; }

python3 "$work/host.py" serve "$work" "$port" "$token" "$other_token" &
server_pid=$!
for i in $(seq 1 100); do
    [[ -e "$work/server.ready" ]] && break
    sleep 0.1
done
[[ -e "$work/server.ready" ]] || fail "the host's server did not come up"

# The volume klogsave keeps the boot's log on (8 MiB, formatted by the
# host's brixfs), and a disk without one, formatted later in brixOS (the
# kernel says so in its log: the log grows).
"$work/brixfs-host" create "$work/disk0.img" 8 > /dev/null
head -c 4194304 /dev/zero > "$work/disk1.img"

# brixOS in QEMU with the image $1 and what else the arguments give,
# typed at through a FIFO; the serial line into serial.txt (kept as
# serial-$boots.txt when the machine is stopped).
boots=0
started=0
boot() {
    local image=$1
    shift
    rm -f "$work/input"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    started=$(date +%s.%N)
    qemu-system-x86_64 -m 256M -display none -monitor none -no-reboot -cpu max -serial stdio -kernel "$image" \
        -drive "file=$work/disk0.img,format=raw,if=virtio" -drive "file=$work/disk1.img,format=raw,if=virtio" \
        "$@" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    prompts=0
    marks=0
}
network=(-netdev user,id=n0 -device virtio-net-pci,netdev=n0)
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -a -c -- "$1" || true) < want )); do
        (( tries++ < 900 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
prompts=0
type_line() {
    prompts=$((prompts + 1))
    wait_for '^root@brix:[^#]*# $' "$prompts"
    printf '%s\r' "$1" >&3
}
# The seconds from QEMU's start to the shell's first prompt.
to_prompt() {
    wait_for '^root@brix:[^#]*# $'
    python3 -c 'import sys, time; print("%.1f" % (time.time() - float(sys.argv[1])))' "$started"
}
# The machine powered off through the init (the volumes committed), the
# serial line kept.
power_off() {
    type_line 'impulsectl poweroff'
    local i
    for (( i = 0; i < 300; i++ )); do kill -0 "$qemu_pid" 2> /dev/null || break; sleep 0.1; done
    kill -0 "$qemu_pid" 2> /dev/null && fail "QEMU did not power off"
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
    exec 3>&-
    boots=$((boots + 1))
    cp "$work/serial.txt" "$work/serial-$boots.txt"
}
# What the command $1 says on the terminal, into $work/said.txt: typed
# between two marks, and what stands between them taken off the serial
# line.
marks=0
typed() {
    marks=$((marks + 1))
    type_line "echo MARK-\"BEGIN\"; $1; echo MARK-\"END\""
    wait_for '^MARK-END$' "$marks"
    host part "$work/serial.txt" MARK > "$work/said.txt"
}
# The service's log (in $work/said.txt), looked at again until a line of
# it is $1; then it has $2 lines in all (when given: no line more than
# is to be there).
service_log() {
    local tries=0
    while true; do
        typed 'cat /temporary/logs/logpush.log'
        if grep -q -- "$1" "$work/said.txt" && [[ -z "${2:-}" || $(wc -l < "$work/said.txt") -ge "$2" ]]; then break; fi
        (( tries++ < 80 )) || fail "no \"$1\" in the service's log, or fewer than ${2:-1} lines: $(cat "$work/said.txt")"
        sleep 0.5
    done
    [[ -z "${2:-}" || $(wc -l < "$work/said.txt") == "$2" ]] || fail "the service's log has $(wc -l < "$work/said.txt") lines, not $2: $(cat "$work/said.txt")"
}
# The path of the upload a line of $work/said.txt names (the first such
# line).
named() { sed -n 's/^logpush: uploaded the kernel log ([0-9]* bytes) as \(brix[^ ]*\) at .*/\1/p' "$work/said.txt" | head -n 1; }
# $work/said.txt is the lines given (each a pattern for one whole line),
# in their order, and no line more.
said_is() {
    local count=$# line=1 pattern
    [[ $(wc -l < "$work/said.txt") == "$count" ]] || fail "$(wc -l < "$work/said.txt") lines were said, not $count: $(cat "$work/said.txt")"
    for pattern in "$@"; do
        sed -n "${line}p" "$work/said.txt" | grep -q -- "$pattern" || fail "line $line said is not \"$pattern\": $(sed -n "${line}p" "$work/said.txt")"
        line=$((line + 1))
    done
}
uploaded='^logpush: uploaded the kernel log ([0-9]* bytes) as brix/[0-9TZ-]*/klog-[0-9.]*s\.txt'

# ---- The first boot: the service by itself, by hand, once more ----
boot "$work/logs.img" "${network[@]}"
with_settings=$(to_prompt)
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
first=$(host wait "$work" 'brix/*/klog-*s.txt' 1 90) || fail "the service's upload after the boot"
[[ $(host count "$work") == 2 && $(host count "$work" PUT) == 1 ]] || fail "the service's first run made $(host count "$work") requests, not the question about the repository and one upload (no log of a boot before, no ROM)"
service_log "^logpush: uploaded the kernel log ([0-9]* bytes) as ${first//./\\.} at the test endpoint http://10\\.0\\.2\\.2:$port (no TLS)\$" 1
echo "ok   the service uploads the kernel log by itself once the network has its address: it asks whether the repository is private, then one upload, $first, and its log says so in one line that names the test endpoint and that it has no TLS"

# What another user's program may read: not the settings, not the
# accounts' passwords, not the disk's blocks; the kernel log and the
# service's log, yes. Root reads all.
typed 'reader 1000 /system/settings/logpush /system/settings/shadow /devices/disk0 /devices/klog /temporary/logs/logpush.log; reader 0 /system/settings/logpush /devices/disk0'
said_is '^reader 1000: /system/settings/logpush: denied$' '^reader 1000: /system/settings/shadow: denied$' '^reader 1000: /devices/disk0: denied$' '^reader 1000: /devices/klog: read [1-9][0-9]* bytes$' '^reader 1000: /temporary/logs/logpush.log: read [1-9][0-9]* bytes$' '^reader 0: /system/settings/logpush: read [1-9][0-9]* bytes$' '^reader 0: /devices/disk0: read 4096 bytes$'
echo "ok   the settings with the token are root's alone to read, as are the passwords and the disk's blocks: a program of the user 1000 is denied them, and reads the kernel log and the service's log"

# The log grows (a volume formatted: the kernel says so), then by hand in
# root's shell: logpush reads the settings and uploads itself.
typed 'brixfs format /devices/disk1; echo formatted $?'
grep -q '^formatted 0$' "$work/said.txt" || fail "the second disk was not formatted: $(cat "$work/said.txt")"
typed 'logpush; echo by-hand $?'
said_is "$uploaded at the test endpoint http://10\\.0\\.2\\.2:$port (no TLS)\$" '^by-hand 0$'
host wait "$work" 'brix/*/klog-*s.txt' 2 30 > /dev/null || fail "the upload by hand"
second=$(named)
grown=$(host grown "$work" "$first" "$second" 'brixOS: disk1: a brixfs volume of ') || fail "the upload by hand against the service's"
echo "ok   logpush by hand uploads the log as it is then under another name ($second): the first upload and $grown bytes more, with the volume the kernel mounted meanwhile"

# By hand as the person logged in is (the user 1000, who may not read
# the settings): the service is asked, uploads, and its lines are said on
# the asker's terminal; asked again at once, it says when it will.
typed 'reader 1000 run logpush; echo asked $?; reader 1000 run logpush; echo again $?'
said_is "$uploaded at the test endpoint http://10\\.0\\.2\\.2:$port (no TLS)\$" '^asked 0$' '^logpush: an upload was made a moment ago: asked again in [1-3] s, the service uploads again$' '^again 1$'
host wait "$work" 'brix/*/klog-*s.txt' 3 30 > /dev/null || fail "the upload the service was asked for"
asked=$(named)
[[ -n "$asked" && "$asked" != "$second" && -f "$work/store/$asked" ]] || fail "the asker was not told the name of an upload of its own ($asked): $(cat "$work/said.txt")"
echo "ok   logpush by hand as the user 1000, who may not read the settings, asks the service: it uploads ($asked) and its line is said on the asker's terminal (0); asked again at once it says when it uploads again (1)"

# Once more by itself, the settings' 15 seconds after its first; then it
# stays for who asks (a oneshot that has not ended: the init's "starting").
host wait "$work" 'brix/*/klog-*s.txt' 4 60 > /dev/null || fail "the service's second upload"
# (Its log: its first upload, the one it was asked for, the asker told to
# wait, its second.)
service_log "$uploaded" 4
fourth=$(sed -n 's/^logpush: uploaded the kernel log ([0-9]* bytes) as \(brix[^ ]*\) at .*/\1/p' "$work/said.txt" | grep -v -x -e "$first" -e "$asked" | head -n 1)
[[ -n "$fourth" && $(grep -c '^logpush: uploaded the kernel log ' "$work/said.txt") == 3 ]] || fail "the service's log does not have its two uploads and the one it was asked for: $(cat "$work/said.txt")"
host grown "$work" "$first" "$fourth" 'brixOS: disk1: a brixfs volume of ' > /dev/null || fail "the service's second upload against its first"
typed 'impulsectl status'
grep -q '^logpush  *starting  *[0-9]* *0 *[0-9]*s  *upload the kernel log into its owner.s private repository$' "$work/said.txt" || fail "the service is not there for askers after its uploads: $(cat "$work/said.txt")"
names=$(host names "$work" 'brix/*/klog-*s.txt') || fail "the names of the four uploads"
echo "ok   the service uploads once more after the settings' 15 s, with what the kernel said since, and stays for who asks: ${names%% *} files in ${names#* }, named by the seconds since the boot"

# The uploads against the log itself.
type_line 'echo KLOG-"BEGIN"; cat /devices/klog; echo KLOG-"END"'
wait_for '^KLOG-END$'
said=$(host klog "$work" "$work/serial.txt" "$fourth") || fail "the uploaded log against /devices/klog"
host klog "$work" "$work/serial.txt" "$first" > /dev/null || fail "the first uploaded log against /devices/klog"
checked=$(host problems "$work") || fail "the requests' headers and bodies"
# (The kernel notes in its log each call of the Linux layer a program
# makes for the first time: logpush makes none, its network is the
# system's own calls.)
output | grep -a -q '^linux call: logpush ' && fail "logpush reaches for the Linux layer: $(output | grep -a '^linux call: logpush ' | head -3)"
echo "ok   the uploads decode to the kernel log of this boot: $said; all $checked requests (the question about the repository before each upload) carried the token as Bearer, the User-Agent, Accept, API version and a JSON body as GitHub's API asks; logpush made no call of the Linux layer"

# With the service stopped nobody can be asked: said, with where to look.
typed 'impulsectl stop logpush; reader 1000 run logpush; echo alone $?'
grep -q '^logpush: the settings (/system/settings/logpush) hold a token and are the system.s alone to read, and the service that uploads for everyone else is not running (it ends at once in an image without settings: /temporary/logs/logpush.log has what it said)$' "$work/said.txt" || fail "what logpush says to a user without the service: $(cat "$work/said.txt")"
grep -q '^alone 1$' "$work/said.txt" || fail "logpush without settings to read and without a service did not end with 1"
echo "ok   with the service stopped, logpush as the user 1000 says that the settings are the system's alone and nobody is there to ask (1)"
power_off
first_boot=$(dirname "$first")

# ---- The second boot: the log of the boot before ----
boot "$work/logs.img" "${network[@]}"
kept=$(host wait "$work" 'brix/*/previous-boot-klog.txt' 1 90) || fail "the upload of the log of the boot before"
[[ $(dirname "$kept") != "$first_boot" ]] || fail "the log of the boot before is filed under that boot ($kept), not under this one"
size=$(host previous "$work" "$kept" "$first") || fail "the log of the boot before against that boot's own upload"
service_log "^logpush: uploaded the log of the boot before (boot\\.[a-z.]*log of disk0, $size bytes) as ${kept//./\\.} at the test endpoint" 2
typed 'logpush; echo by-hand $?'
said_is "$uploaded at the test endpoint" "^logpush: the log of the boot before is there already (${kept//./\\.})\$" '^by-hand 0$'
[[ $(ls "$work"/store/brix/*/previous-boot-klog.txt | wc -l) == 1 ]] || fail "the log of the boot before was uploaded more than once"
# (Nothing happens in this boot that the kernel says: when the time of
# the service's second upload comes, the log is what it uploaded.)
before=$(host count "$work")
service_log '^logpush: the kernel log has not grown in 15 s: nothing more to upload$' 3
[[ $(host count "$work") == "$before" ]] || fail "the service sent something though the kernel log had not grown"
echo "ok   a second boot uploads the log klogsave kept of the first ($kept, $size bytes: the beginning of the first boot's own upload), once: by hand it is there already; the kernel log does not grow in this boot, and the service says so 15 s later and uploads nothing more"
power_off

# ---- A try again does not upload again what went ----
# The repository takes the kernel log and not the log of the boot before
# (GitHub's side twice, then refused): the service tries again 5 and 10
# seconds later for that log alone. The kernel log it uploaded stays one
# file. And the second upload of the boot comes all the same, the
# settings' 10 seconds later: for the log of the boot before, which the
# server takes now (the kernel log has not grown: it is not uploaded
# again).
boot "$work/shaky.img" "${network[@]}"
failed_side='^logpush: the log of the boot before was not uploaded: 500: GitHub.s side failed; GitHub: "Server Error"$'
service_log '^logpush: uploaded the log of the boot before (boot\.[a-z.]*log of disk0, [0-9]* bytes) as brix/[0-9TZ-]*/previous-boot-klog\.txt at the test endpoint' 7
said_is "$uploaded at the test endpoint" "$failed_side" '^logpush: trying again in 5 s (try 2 of 5)$' "$failed_side" '^logpush: trying again in 10 s (try 3 of 5)$' '^logpush: the log of the boot before was not uploaded: 422: GitHub does not take the file (one of this name is there already, or the branch is not there); GitHub: "Invalid request\."$' '^logpush: uploaded the log of the boot before '
shaky_first=$(named)
shaky_boot=$(dirname "$shaky_first")
[[ -n "$shaky_first" && $(ls "$work/store/$shaky_boot"/klog-*s.txt | wc -l) == 1 ]] || fail "$(ls "$work/store/$shaky_boot"/klog-*s.txt 2> /dev/null | wc -l) files of the kernel log in $shaky_boot, not the one the first try uploaded: a try again uploaded the kernel log again"
[[ -f "$work/store/$shaky_boot/previous-boot-klog.txt" ]] || fail "the log of the boot before did not arrive with the second upload"
echo "ok   a server that fails the log of the boot before (500, 500, then 422): tried again 5 and 10 s later for that log alone, the kernel log stays the one file $shaky_first; the second upload of the boot comes 10 s later all the same and brings the log of the boot before"
power_off

# ---- A server that does not take the token ----
before=$(host count "$work")
boot "$work/denied.img" "${network[@]}"
refused='^logpush: nothing was uploaded: 401: GitHub does not take the token (mistyped, expired or revoked): the settings need a new one; GitHub: "Bad credentials"$'
service_log "$refused" 1
typed 'logpush; echo by-hand $?'
said_is "$refused" '^by-hand 1$'
typed 'reader 1000 run logpush; echo asked $?'
said_is "$refused" '^asked 1$'
(( $(host count "$work") == before + 3 )) || fail "a token that is not taken was tried $(( $(host count "$work") - before )) times, not once by the service, once by hand and once for an asker"
# (The service's log: its own try and the asker's, neither tried again.)
service_log "$refused" 2
# The token in nothing written: the service's log and the kernel log are
# put on the serial line, and every boot's serial line searched.
typed 'cat /temporary/logs/logpush.log /devices/klog; echo shown $?'
grep -q '^shown 0$' "$work/said.txt" || fail "the logs were not shown"
# (This boot's saved log made long, 349 KiB of numbers: the next boot's
# "log of the boot before", of which logpush takes the kernel log's own
# size, a quarter of a megabyte.)
typed 'seq 60000 > /volumes/disk0/boot.log; sync; echo long $?'
grep -q '^long 0$' "$work/said.txt" || fail "the long log was not written: $(cat "$work/said.txt")"
power_off
# (The boot whose kernel log was shown on the serial line: looked at
# again below for what the kernel says of its random numbers.)
shown_boot=$boots
searched=$(host absent "$work" "$token" "$work"/serial-*.txt) || fail "the token"
echo "ok   a server that answers 401: the reason in the service's log, on the terminal (1) and to an asker (1), tried once each and not again; the token is in none of the $searched boots' serial lines, with the service's log and the kernel log shown on them"

# ---- Over TLS ----
# TLS in front of the server, with a root and a certificate for 10.0.2.2
# made now; the root is what one image trusts.
"$work/tlsfront" "$tls_port" "$port" "$work/root.pem" 10.0.2.2 > "$work/tlsfront.txt" 2>&1 &
front_pid=$!
for i in $(seq 1 100); do
    grep -q '^tlsfront: ready$' "$work/tlsfront.txt" 2> /dev/null && break
    sleep 0.1
done
grep -q '^tlsfront: ready$' "$work/tlsfront.txt" || fail "the TLS front did not come up: $(cat "$work/tlsfront.txt")"
printf 'o/logs\n%s\nagain 0\napi https://10.0.2.2:%s\n' "$other_token" "$tls_port" > "$work/settings-tls"
"$work/brix-image" --add "$work/none.img" "$work/tls.img" "settings/logpush=$work/settings-tls" "settings/certificates=$work/root.pem"
"$work/brix-image" --add "$work/none.img" "$work/tls-github.img" "settings/logpush=$work/settings-tls" "settings/certificates=projects/brix/settings/certificates"

# (Two uploads: the kernel log, and the log klogsave kept of the boot
# before this one.)
before=$(host count "$work" PUT)
boot "$work/tls.img" "${network[@]}"
service_log "^logpush: uploaded the log of the boot before (boot\\.[a-z.]*log of disk0, 262144 bytes) as brix/[0-9TZ-]*/previous-boot-klog\\.txt at 10\\.0\\.2\\.2:$tls_port\$" 2
said_is "$uploaded at 10\\.0\\.2\\.2:$tls_port\$" "^logpush: uploaded the log of the boot before "
(( $(host count "$work" PUT) == before + 2 )) || fail "the uploads over TLS: $(( $(host count "$work" PUT) - before )) reached the server, not the kernel log and the log of the boot before"
long=$(sed -n 's/^logpush: uploaded the log of the boot before ([^)]*) as \(brix[^ ]*\) at .*/\1/p' "$work/said.txt")
host long "$work" "$long" > /dev/null || fail "the long log of the boot before as it arrived over TLS"
power_off
host problems "$work" > /dev/null || fail "the requests that came over TLS"
echo "ok   over TLS (std's TLS server on the host, a certificate for 10.0.2.2 under a root the image has as its trusted certificates), with a token that is no test's: the kernel log and a log of the boot before of 262144 bytes go through byte for byte, each said in one line that names the host"

# The same server, and the image trusts what the build's image trusts.
before=$(host count "$work")
boot "$work/tls-github.img" "${network[@]}"
service_log "^logpush: nothing was uploaded: the certificate of 10\\.0\\.2\\.2:$tls_port is not trusted: no certificate authority this system trusts has signed it\$" 1
power_off
# The machine's clock before this program was written, and after the
# certificate's last day.
boot "$work/tls.img" "${network[@]}" -rtc base=2000-01-01T00:00:00
service_log "^logpush: nothing was uploaded: this machine.s clock is not set (it says 2000-01-01), and without the date the certificate of 10\\.0\\.2\\.2:$tls_port cannot be judged: set the date and the time (UTC) in the firmware.s setup\$" 1
power_off
boot "$work/tls.img" "${network[@]}" -rtc base=2036-03-01T00:00:00
service_log "^logpush: nothing was uploaded: the certificate of 10\\.0\\.2\\.2:$tls_port is not trusted: it has expired by this machine.s clock, which says 2036-03-01\$" 1
power_off
# A processor without RDRAND and RDSEED: the kernel says in its log what
# its random numbers are made of, and logpush sends no token over a
# connection whose key would be made of the clock.
boot "$work/tls.img" "${network[@]}" -cpu qemu64
service_log '^logpush: nothing was uploaded: this machine.s processor has no source of random numbers, and a TLS key made without one could be worked out by someone who listens: the token is not sent$' 1
typed 'cat /devices/klog'
grep -q '^brixOS: random numbers keyed from the clock alone (the processor has no source of its own): not fit for secrets$' "$work/said.txt" || fail "the kernel does not say that its random numbers are from the clock alone (-cpu qemu64 has no RDRAND)"
power_off
grep -a -q '^brixOS: random numbers keyed from the processor.s own source' "$work/serial-$shown_boot.txt" || fail "the kernel does not say that its random numbers are keyed from the processor's source (-cpu max has RDRAND and RDSEED)"
[[ $(host count "$work") == "$before" ]] || fail "something was sent to a server whose certificate was not trusted, or could not be judged, or from a machine without random numbers"
host absent "$work" "$other_token" "$work"/serial-*.txt > /dev/null || fail "the token of the uploads over TLS"
echo "ok   the same server against the roots of the build's image (GitHub's authorities) is not trusted; with the clock at 2000 the certificate cannot be judged, at 2036 it has expired; on a processor without a source of random numbers no TLS key is made (the kernel's log says what its numbers are keyed from): each said in one line with the reason, nothing sent, the token on no serial line"

# ---- No settings; no network device; the boot's time ----
before=$(host count "$work")
boot "$work/none.img" "${network[@]}"
with_service=$(to_prompt)
service_log '^logpush: no settings (/system/settings/logpush): nothing is uploaded from this machine$' 1
typed 'impulsectl status'
grep -q '^logpush  *done  *-  *0 *-  *upload the kernel log' "$work/said.txt" || fail "without settings the service is not done: $(cat "$work/said.txt")"
typed 'logpush; echo by-hand $?; reader 1000 run logpush; echo asked $?'
said_is '^logpush: no settings (/system/settings/logpush): nothing is uploaded from this machine; projects/brix/README.md says how an image gets them$' '^by-hand 1$' '^logpush: no settings (/system/settings/logpush): nothing is uploaded from this machine; projects/brix/README.md says how an image gets them$' '^asked 1$'
[[ $(host count "$work") == "$before" ]] || fail "an image without settings sent something"
power_off
echo "ok   an image without settings: one line in the service's log, the service done (no error), nothing sent; by hand the same line (1), for root and for another user"

boot "$work/logs.img"
service_log '^logpush: nothing is uploaded: this machine has no network device$' 1
typed 'reader 1000 run logpush; echo asked $?'
said_is '^logpush: nothing was uploaded: this machine has no network device$' '^asked 1$'
power_off
echo "ok   a machine without a network device: said in the service's log and to who asks (1), nothing sent"

boot "$work/plain.img" "${network[@]}"
without_service=$(to_prompt)
power_off
python3 - "$with_service" "$without_service" <<'PY' || fail "the boot is slower with the service: $with_service s to the shell's prompt against $without_service s without it"
import sys
with_service, without = float(sys.argv[1]), float(sys.argv[2])
sys.exit(0 if with_service <= without * 1.5 + 3 else 1)
PY
echo "ok   the boot does not wait for it: $with_service s from QEMU's start to the shell's prompt with the service (no settings), $with_settings s with settings and an upload under way, $without_service s without the service"
echo PASS
