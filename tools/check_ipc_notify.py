# Plays systemd's notify socket for mlx-ipcd (tools/check_ipc.sh):
# READY=1 must come, and the bus socket must exist when it does; a
# notify socket by path and an abstract one ("@name").
#
#   python3 tools/check_ipc_notify.py MLX_IPCD WORK_DIRECTORY EMPTY_DIRECTORY
import os, socket, subprocess, sys, time
ipcd, work, empty = sys.argv[1], sys.argv[2], sys.argv[3]
for abstract in (False, True):
    name = "@mlx-ipcd-notify-test" if abstract else os.path.join(work, "notify")
    if not abstract and os.path.exists(name):
        os.unlink(name)
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    listener.bind("\0" + name[1:] if abstract else name)
    listener.settimeout(10)
    bus = os.path.join(work, "bus-abstract" if abstract else "bus-path")
    environment = dict(os.environ, NOTIFY_SOCKET=name, XDG_DATA_DIRS=empty)
    process = subprocess.Popen([ipcd, "--system", "--address", bus], env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        message = listener.recv(256)
        there = os.path.exists(bus)
    finally:
        process.terminate()
        process.wait(5)
    if message != b"READY=1" or not there:
        print("FAILED", "abstract" if abstract else "path", message, there)
        sys.exit(1)
    print("ok  ", "abstract" if abstract else "path", "socket: READY=1 once the bus listens")
