# GNU coreutils compatibility

The behavioral target is the locally installed GNU coreutils 9.11. Option
names and contracts come from the matching files in `/usr/share/man/man1` and
the installed `--help` output. Tests compare MLX executables directly with
their `/usr/bin` counterparts.

## Implemented utilities

| Utility | Option surface | Remaining semantic work |
| --- | --- | --- |
| `true`, `false` | complete | none known |
| `echo` | complete | locale-specific XSI behavior is not enabled |
| `cat` | complete | validate multibyte display behavior outside `LC_ALL=C` |
| `wc` | complete | full Unicode `wcwidth` and hardware-specific `--debug` wording |
| `pwd` | complete | none known |
| `mkdir` | complete | native context assignment still needs validation on an SELinux/SMACK host |
| `rmdir` | complete | none known |
| `basename`, `dirname` | complete | none known |
| `head` | complete | bounded-memory streaming for negative counts |
| `tail` | complete | inotify, multiple followed files, and repeated `--pid` values currently use reduced polling behavior |
| `tee` | complete | validate rare non-EPIPE device failures for every output-error mode |
| `yes` | complete | none known |
| `sleep` | complete | GNU infinity and scientific-number extensions |
| `uname` | complete | platform/processor discovery where the kernel reports extra values |
| `printenv` | complete | none known |
| `env` | complete | full FreeBSD `-S` expansion grammar and inherited signal-state detail |
| `nproc` | complete | OpenMP thread-limit environment extensions |
| `link`, `unlink` | complete | none known |
| `touch` | complete | full GNU natural-language dates and non-UTC local timezone rules |
| `truncate` | complete | validate filesystem-specific IO-block behavior |
| `mkfifo` | complete | native context assignment still needs validation on an SELinux/SMACK host |
| `sync` | complete | none known |
| `cp` | partial | recursive trees, symlink policies, backups, sparse/reflink copies, ownership, xattrs, ACLs, and interactive mode |
| `mv` | partial | cross-filesystem copy fallback, backups, interactive mode, full update modes, and native context assignment |
| `rm` | partial | unwritable-file terminal prompts, `--preserve-root=all` parent-device checks, and exact GNU interactive directory wording |
| `ln` | partial | numbered/existing backup policies and atomic replacement without backups |
| `chmod` | partial | recursive `-L` cycle-safe traversal, recursive `--dereference` of encountered symlinks, and exact verbose diagnostics |
| `readlink` | complete | exact POSIXLY_CORRECT diagnostic defaults and the multiple-file `-n` warning |
| `realpath` | complete | paths are currently bounded to 64 KiB and exact platform-specific diagnostics differ |
| `chown`, `chgrp` | partial | recursive `-L` cycle-safe traversal, recursive dereference of encountered symlinks, and NSS sources beyond local passwd/group files |
| `stat` | partial | locale-aware human timestamps, mount-point discovery, SELinux contexts, printf width/precision flags, and exact `--cached` synchronization policy |
| `ls` | partial | local-zone/default time styles, aligned named identities, terminal-aware columns/color/hyperlinks, full quoting/glob ignores, block scaling, and recursive symlink cycle detection |

“Complete” means the documented option surface is implemented and covered by
the current parity suite. It does not claim locale, filesystem, or kernel
behaviors that cannot be exercised on the development host.

## Utilities not implemented yet

The installed GNU 9.11 suite contains 103 command names. The initial
filesystem-tool pass is complete; implementation now proceeds through
text/data tools, checksums, and the remaining system/account utilities.
