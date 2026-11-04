# tar

Archives (POSIX ustar, `std.tar`) made, listed and extracted, plain or
gzip (`std.flate.gzip`), on Linux and brixOS:

    tar -cf ARCHIVE [-z] [-C DIR] PATH...   PATHs into ARCHIVE (directories
                                            with everything in them, in
                                            name order; files, directories
                                            and symbolic links)
    tar -xf ARCHIVE [-z] [-C DIR]           ARCHIVE's members into the
                                            current (or DIR) directory
    tar -tf ARCHIVE [-z]                    the member names

The letters go together, with or without the dash (`tar -czf out.tar.gz
std`, `tar xzf out.tar.gz`); `-v` names each member as it is handled, `-z`
makes a gzip archive (reading, a gzip archive is told from a plain one by
itself); ARCHIVE `-` is standard output or input (`tar -cf - std | tar -xf
- -C /volumes/disk1`).

What is kept: names (ustar's prefix field when longer than 100 bytes; GNU
tar's long names are read), modes of files and directories, link targets.
What is not: owners, times, hard links, devices. Extracting, a name that
leaves the directory (`..`, a leading slash) is passed over with a
message, as a safety against an archive from elsewhere.

`std.tar` (`std/src/tar.mlx`) is the format: `parseHeader` and
`writeHeader` for the 512-byte header blocks, and `reader`/`next` over an
archive in memory (regular files only). Built as `mlx-out/bin/tar` and,
for brixOS, `mlx-out/bin/brix/tar` (in the boot image);
tools/check_brix_selfhost.sh moves the compiler's sources into brixOS
with it.
