#!/usr/bin/env python3
"""Small mlx-init control-socket fixture for the mlx-login check."""

import os
import socket
import sys


def main() -> int:
    if len(sys.argv) != 3:
        return 2
    path, log_path = sys.argv[1:]
    try:
        os.unlink(path)
    except FileNotFoundError:
        pass
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(path)
    os.chmod(path, 0o600)
    server.listen(4)
    while True:
        connection, _ = server.accept()
        with connection:
            request = bytearray()
            while b"\n" not in request and len(request) < 256:
                part = connection.recv(256 - len(request))
                if not part:
                    break
                request.extend(part)
            verb = bytes(request).split(b"\n", 1)[0].strip()
            if verb in (b"poweroff", b"reboot"):
                with open(log_path, "ab") as output:
                    output.write(verb + b"\n")
                connection.sendall(b"ok\n")
            elif verb:
                connection.sendall(b"error: unknown request\n")


if __name__ == "__main__":
    raise SystemExit(main())
