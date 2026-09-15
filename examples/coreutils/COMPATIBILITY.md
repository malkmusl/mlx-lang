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
| `tail` | partial | `--follow`, `--retry`, `--pid`, `--sleep-interval`, and `--max-unchanged-stats` |
| `tee` | complete | validate rare non-EPIPE device failures for every output-error mode |
| `yes` | complete | none known |
| `sleep` | complete | GNU infinity and scientific-number extensions |
| `uname` | complete | platform/processor discovery where the kernel reports extra values |
| `printenv` | complete | none known |
| `env` | partial | `--argv0`, `--chdir`, `--split-string`, signal-list operations, and debug tracing |
| `nproc` | complete | OpenMP thread-limit environment extensions |
| `link`, `unlink` | complete | none known |
| `touch` | partial | selected timestamps, references, parsed dates, explicit timestamps, and symlink mode |
| `truncate` | complete | validate filesystem-specific IO-block behavior |
| `mkfifo` | complete | native context assignment still needs validation on an SELinux/SMACK host |
| `sync` | complete | none known |

“Complete” means the documented option surface is implemented and covered by
the current parity suite. It does not claim locale, filesystem, or kernel
behaviors that cannot be exercised on the development host.

## Utilities not implemented yet

The installed GNU 9.11 suite contains 103 command names. After existing flag
parity is closed, implementation proceeds through filesystem tools (`cp`,
`mv`, `rm`, `ln`, `chmod`, `chown`, `stat`, `readlink`, `realpath`, `ls`), then
text/data tools, checksums, and the remaining system/account utilities.
