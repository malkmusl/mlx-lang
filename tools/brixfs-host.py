#!/usr/bin/env python3
# brixfs-host: read a brixOS brixfs volume from Linux (or any host), so the
# files brixOS writes -- the boot log, anything -- are reachable from the
# host without brixOS having to speak a foreign filesystem. Read-only, no
# dependencies (just Python 3). Point it at a brixfs partition (e.g.
# /dev/sdc2), a whole GPT disk (it finds the brixfs partition), or an image
# file.
#
#   brixfs-host.py DEVICE ls [PATH]
#   brixfs-host.py DEVICE cat PATH
#   brixfs-host.py DEVICE tree [PATH]
#   brixfs-host.py DEVICE extract PATH DEST
#
# The on-disk format is projects/brix/brixfs.mlx; this mirrors it. brixfs
# is copy-on-write: two superblocks (blocks 0 and 1) take turns and the
# valid one with the higher generation is the volume.
import sys, os, struct, stat

BLOCK = 4096
MAGIC = 0x3130534658495242  # "BRIXFS01"
VERSION = 2
INODE_SIZE = 256
INODES_PER_BLOCK = BLOCK // INODE_SIZE          # 16
POINTERS_PER_BLOCK = BLOCK // 8                  # 512
EXTENT_SIZE = 16
INLINE_EXTENTS = 12
EXTENTS_PER_BLOCK = BLOCK // EXTENT_SIZE          # 256
ENTRY_SIZE = 64
ROOT_INODE = 1
# inode kinds
FREE, DIRECTORY, FILE, SYMLINK, FIFO = 0, 1, 2, 3, 4

FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK64 = (1 << 64) - 1


def u(b, at, size):
    v = 0
    for k in range(size):
        v |= b[at + k] << (8 * k)
    return v


def fnv_checksum(block64, skip):
    # FNV-1a over the first 64 bytes, the 8 at `skip` read as zero.
    h = FNV_OFFSET
    for k in range(64):
        byte = 0 if skip <= k < skip + 8 else block64[k]
        h = ((h ^ byte) * FNV_PRIME) & MASK64
    return h


class Volume:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.base = self._find_base()
        sb = self._superblock()
        if sb is None:
            raise SystemExit("brixfs-host: no valid brixfs superblock found on " + path)
        self.blocks, self.generation, self.root, self.next_inode = sb

    def _read_raw(self, off, size):
        self.f.seek(off)
        return self.f.read(size)

    def block(self, n):
        return self._read_raw(self.base + n * BLOCK, BLOCK)

    def _decode_sb(self, b):
        if len(b) < 64:
            return None
        if u(b, 0, 8) != MAGIC or u(b, 8, 8) != VERSION:
            return None
        if u(b, 56, 8) != fnv_checksum(b[:64], 56):
            return None
        return (u(b, 16, 8), u(b, 24, 8), u(b, 32, 8), u(b, 40, 8))  # blocks, gen, root, next

    def _superblock(self):
        best = None
        for n in (0, 1):
            sb = self._decode_sb(self._read_raw(self.base + n * BLOCK, BLOCK))
            if sb and (best is None or sb[1] > best[1]):
                best = sb
        return best

    def _find_base(self):
        # The volume itself (offset 0), else a brixfs partition of a GPT disk.
        if self._decode_sb(self._read_raw(0, BLOCK)):
            return 0
        head = self._read_raw(512, 92)
        if len(head) >= 92 and head[0:8] == b"EFI PART":
            entries_lba = u(head, 72, 8)
            count = u(head, 80, 4)
            size = u(head, 84, 4)
            for i in range(count):
                e = self._read_raw(entries_lba * 512 + i * size, 128)
                if len(e) < 56 or e[0:16] == b"\x00" * 16:
                    continue
                first = u(e, 32, 8)
                if self._decode_sb(self._read_raw(first * 512, BLOCK)):
                    return first * 512
        return 0

    def inode(self, number):
        pb = self.block(self.root)
        ib_num = u(pb, (number // INODES_PER_BLOCK) * 8, 8)
        if ib_num == 0:
            return None
        ib = self.block(ib_num)
        at = (number % INODES_PER_BLOCK) * INODE_SIZE
        kind = u(ib, at + 0, 1)
        if kind == FREE:
            return None
        node = {
            "kind": kind,
            "mode": u(ib, at + 2, 2),
            "links": u(ib, at + 4, 4),
            "size": u(ib, at + 8, 8),
            "user": u(ib, at + 40, 4),
            "group": u(ib, at + 44, 4),
            "extentCount": u(ib, at + 48, 4),
            "depth": u(ib, at + 52, 1),
            "tree": u(ib, at + 56, 8),
            "inline": ib[at + 64: at + 64 + INLINE_EXTENTS * EXTENT_SIZE],
        }
        return node

    def _extent(self, node, k):
        if k >= node["extentCount"]:
            return None
        if node["depth"] == 0:
            if k >= INLINE_EXTENTS:
                return None
            b = node["inline"]
            o = k * EXTENT_SIZE
            return (u(b, o, 4), u(b, o + 4, 4), u(b, o + 8, 8))
        block = node["tree"]
        if node["depth"] == 2:
            pb = self.block(node["tree"])
            block = u(pb, (k // EXTENTS_PER_BLOCK) * 8, 8)
            if block == 0:
                return None
        eb = self.block(block)
        o = (k % EXTENTS_PER_BLOCK) * EXTENT_SIZE
        return (u(eb, o, 4), u(eb, o + 4, 4), u(eb, o + 8, 8))

    def read_file(self, node):
        size = node["size"]
        data = bytearray(size)
        for k in range(node["extentCount"]):
            e = self._extent(node, k)
            if e is None:
                break
            start, length, disk = e
            for j in range(length):
                file_block = start + j
                fo = file_block * BLOCK
                if fo >= size:
                    continue
                chunk = self.block(disk + j)
                n = min(BLOCK, size - fo)
                data[fo:fo + n] = chunk[:n]
        return bytes(data)

    def list_dir(self, node):
        raw = self.read_file(node)
        out = []
        for off in range(0, len(raw), ENTRY_SIZE):
            if off + 5 > len(raw):
                break
            ino = u(raw, off, 4)
            if ino == 0:
                continue
            nl = u(raw, off + 4, 1)
            name = raw[off + 5: off + 5 + nl].decode("utf-8", "replace")
            if name in (".", ".."):
                continue
            out.append((name, ino))
        return out

    def resolve(self, path):
        node_no = ROOT_INODE
        node = self.inode(node_no)
        for part in [p for p in path.split("/") if p]:
            if node is None or node["kind"] != DIRECTORY:
                return None, None
            nxt = None
            for name, ino in self.list_dir(node):
                if name == part:
                    nxt = ino
                    break
            if nxt is None:
                return None, None
            node_no = nxt
            node = self.inode(node_no)
        return node_no, node


def kindname(k):
    return {DIRECTORY: "dir", FILE: "file", SYMLINK: "link", FIFO: "fifo"}.get(k, "?")


def cmd_ls(vol, path):
    no, node = vol.resolve(path)
    if node is None:
        raise SystemExit("brixfs-host: no such path: " + path)
    if node["kind"] != DIRECTORY:
        print("%-5s %10d  %s" % (kindname(node["kind"]), node["size"], path))
        return
    for name, ino in sorted(vol.list_dir(node)):
        child = vol.inode(ino)
        if child is None:
            continue
        print("%-5s %10d  %s" % (kindname(child["kind"]), child["size"], name))


def cmd_cat(vol, path):
    no, node = vol.resolve(path)
    if node is None or node["kind"] not in (FILE, SYMLINK):
        raise SystemExit("brixfs-host: not a file: " + path)
    sys.stdout.buffer.write(vol.read_file(node))


def cmd_tree(vol, path, prefix=""):
    no, node = vol.resolve(path)
    if node is None:
        raise SystemExit("brixfs-host: no such path: " + path)
    if node["kind"] != DIRECTORY:
        print(path)
        return
    entries = sorted(vol.list_dir(node))
    for i, (name, ino) in enumerate(entries):
        child = vol.inode(ino)
        last = i == len(entries) - 1
        print(prefix + ("└── " if last else "├── ") + name + ("/" if child and child["kind"] == DIRECTORY else ""))
        if child and child["kind"] == DIRECTORY:
            cmd_tree(vol, (path.rstrip("/") + "/" + name), prefix + ("    " if last else "│   "))


def cmd_extract(vol, path, dest):
    no, node = vol.resolve(path)
    if node is None:
        raise SystemExit("brixfs-host: no such path: " + path)
    if node["kind"] == DIRECTORY:
        os.makedirs(dest, exist_ok=True)
        for name, ino in vol.list_dir(node):
            cmd_extract(vol, path.rstrip("/") + "/" + name, os.path.join(dest, name))
    else:
        with open(dest, "wb") as out:
            out.write(vol.read_file(node))


def main():
    if len(sys.argv) < 3:
        print(__doc__ if __doc__ else "usage: brixfs-host.py DEVICE {ls|cat|tree|extract} ...", file=sys.stderr)
        print("usage: brixfs-host.py DEVICE {ls [PATH]|cat PATH|tree [PATH]|extract PATH DEST}", file=sys.stderr)
        sys.exit(2)
    device, op = sys.argv[1], sys.argv[2]
    vol = Volume(device)
    if op == "ls":
        cmd_ls(vol, sys.argv[3] if len(sys.argv) > 3 else "/")
    elif op == "cat":
        cmd_cat(vol, sys.argv[3])
    elif op == "tree":
        cmd_tree(vol, sys.argv[3] if len(sys.argv) > 3 else "/")
    elif op == "extract":
        cmd_extract(vol, sys.argv[3], sys.argv[4])
    else:
        raise SystemExit("brixfs-host: unknown command: " + op)


if __name__ == "__main__":
    main()
