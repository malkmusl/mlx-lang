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
| `arch` | complete | none known |
| `logname` | complete | reads `/proc/self/loginuid`; NSS sources beyond local passwd files |
| `whoami` | complete | NSS sources beyond local passwd files |
| `seq` | partial | integer arguments only; `-f/--format` floating-point printf formatting is not implemented |
| `tac` | partial | `-r/--regex` regular-expression separators are not implemented |
| `nl` | partial | header/body/footer section delimiters (`\:\:\:`), `-p` no-reset, `-bpREGEX` numbering, and `-l` blank-line joining are not implemented; the whole input is treated as one body section |
| `fold` | complete | none known |
| `uniq` | partial | `--all-repeated` and `--group` (blank-line-separated group output) are not implemented |
| `cut` | complete | none known |
| `comm` | partial | `--check-order`/`--nocheck-order` sorted-input validation and `--total` are accepted but not implemented (input order is trusted, not verified) |
| `tr` | partial | `[CHAR*N]` repeat notation and `[=CHAR=]` equivalence classes are not implemented; ranges, POSIX classes, octal/backslash escapes, translate/delete/squeeze/complement all work |
| `expr` | partial | arithmetic uses fixed 64-bit integers rather than GNU's arbitrary precision; `\{n,m\}` interval repetition and `[=CHAR=]`/`[.CHAR.]` in regex bracket expressions are not implemented; arithmetic, comparisons, `\|`/`\&`, `length`/`substr`/`index`/`match`, `:`, and parentheses all work |
| `test`, `[` | partial | `-b`, `-c`, `-p`, `-S`, `-g`, `-u`, `-k`, `-O`, `-G`, `-N` file tests are not implemented; string/numeric comparisons, `-e`/`-f`/`-d`/`-L`/`-h`/`-r`/`-w`/`-x`/`-s`, `-nt`/`-ot`/`-ef`, `!`/`-a`/`-o`, and parentheses all work |
| `base64`, `base32`, `basenc` | partial | decoding does not strictly reject malformed/non-canonically-padded input the way GNU does; `basenc --z85` is not implemented; encoding, decoding, `-w`/`--wrap`, `-i`/`--ignore-garbage`, and all of basenc's `--base64`/`--base64url`/`--base32`/`--base32hex`/`--base16`/`--base2msbf`/`--base2lsbf` modes work |
| `md5sum`, `sha1sum`, `sha224sum`, `sha256sum`, `sha384sum`, `sha512sum` | partial | a checksum file where every listed file is missing (or ignored via `--ignore-missing`) exits 0 instead of GNU's 1 ("no file was verified"); digest computation, `-b`/`-c`/`--tag`/`-t`/`-z`/`--status`/`--quiet`/`--strict`/`--ignore-missing`, and both the standard and BSD-tag checksum-file formats work |
| `sum` | complete | none known |
| `cksum` | partial | `-c`/`--check`, `--base64`, `--raw`, `-l`/`--length`, `-z`/`--zero`, and the `blake2b`/`sm3` digest types are not implemented; the default 32-bit CRC algorithm and `-a`/`--algorithm=`{`crc`,`sysv`,`bsd`,`md5`,`sha1`,`sha224`,`sha256`,`sha384`,`sha512`} with `--tag`/`--untagged` all work |
| `b2sum` | partial | `-l`/`--length` (variable digest length) is not implemented, only the full 512-bit digest; a checksum file where every listed file is missing (or ignored via `--ignore-missing`) exits 0 instead of GNU's 1 ("no file was verified"); digest computation, `-b`/`-c`/`--tag`/`-t`/`-z`/`--status`/`--quiet`/`--strict`/`--ignore-missing`, and both the standard and BSD-tag checksum-file formats work |
| `expand` | partial | the comma-separated `-t`/`--tabs=LIST` custom tab-stop form is not implemented, only a single numeric width; `-i`/`--initial` and `-t`/`--tabs=N` work |
| `unexpand` | partial | the comma-separated `-t`/`--tabs=LIST` custom tab-stop form is not implemented, only a single numeric width; column tracking treats an input blank run as a uniform span (a literal input tab immediately adjacent to a tab stop can be re-emitted as a space instead of preserved verbatim), and other terminal control characters (e.g. backspace) are not given special column-width handling; `-a`/`--all`, `--first-only`, and `-t`/`--tabs=N` work |
| `paste` | complete | none known |
| `shuf` | partial | randomness is drawn from `/dev/urandom` (or `--random-source=FILE`) using a Fisher-Yates shuffle, but the exact bit-consumption pattern does not match GNU's arbitrary-precision generator, so `--random-source` output is not byte-identical to real `shuf` even for the same source file; `-r`/`--repeat` requires `-n`/`--head-count` in this implementation (GNU allows unbounded `-r` output without `-n`); negative `-i`/`--input-range` bounds are not supported; `-e`/`--echo`, `-i` (non-negative), `-n`, `-o`/`--output`, `-z`/`--zero-terminated`, and `--random-source` all work |
| `pr` | partial | multi-column layout (`-COLUMN`, `-a`/`--across`, `-m`/`--merge`) is not implemented, only single-column pagination; `-w`/`-W`/`--page-width` (truncation), `-s`/`-S`/`--separator`/`--sep-string`, `-e`/`-i` (tab expand/output-tabs), `-J`/`--join-lines`, `-c`/`-v` (control-char display), `-D`/`--date-format`, `-N`/`--first-line-number`, `-r`/`--no-file-warnings` (accepted but every file open failure is always reported), and `+FIRST_PAGE[:LAST_PAGE]` are not implemented; page numbering always restarts at 1 for each FILE argument; the header date/time uses a fixed `YYYY-MM-DD HH:MM` format (no locale or `-D` customization); `-l`/`--length`, `-t`/`-T`/`--omit-header`/`--omit-pagination`, `-h`/`--header`, `-n`/`--number-lines[=SEP[DIGITS]]`, `-o`/`--indent`, `-d`/`--double-space`, `-F`/`-f`/`--form-feed`, and pagination across multiple pages/files otherwise match GNU byte-for-byte (verified header/trailer line counts, blank-line page padding, and the header text's date/centered-title/page-number column layout) |
| `csplit` | partial | `-b`/`--suffix-format` is accepted but always uses the default `%02d`-equivalent zero-padded decimal suffix (a custom `sprintf` FORMAT has no effect); `{*}` repetition of a plain `INTEGER` pattern stops gracefully once the next split point would exceed the line count, rather than reproducing GNU's own quirk of erroring out on the repetition that would land exactly at end-of-file; `/REGEXP/` and `%REGEXP%` use a small hand-rolled BRE-subset matcher (literal bytes, `.`, `[...]`/`[^...]` classes with ranges, `*`, and `^`/`$` anchors) shared with `expr`'s pattern engine, not a full POSIX BRE (no `\{m,n\}` intervals, no `\(...\)` backreferences, no equivalence classes); collation/matching is always byte-order; `-k`/`--keep-files`, `--suppress-matched`, `-f`/`--prefix`, `-n`/`--digits`, `-s`/`-z`, `INTEGER` patterns, `{N}`/`{*}` repetition, and `/REGEXP/[OFFSET]`/`%REGEXP%[OFFSET]` all otherwise match GNU byte-for-byte, including its file-deletion-on-error and the `%REGEXP%{*}`-exhaustion-suppresses-the-final-chunk quirk |
| `fmt` | partial | line-break positions use simple greedy fill-to-width rather than GNU's goal-width paragraph-balancing algorithm (GNU targets 93% of width by default and balances breaks across a whole paragraph/long line to avoid a short final line; this implementation packs each line as full as possible before wrapping), so reflowed output rarely matches GNU byte-for-byte even though word order/content, indentation, paragraph boundaries, and the width bound are all preserved correctly; `-c`/`--crown-margin`, `-t`/`--tagged-paragraph`, `-p`/`--prefix`, and `-g`/`--goal` are accepted but have no effect; default (no-flag) paragraph indentation only handles the common case of a single uniform indent per paragraph, not GNU's implicit new-paragraph-on-indent-change detection; `-w`/`--width` (including the `-WIDTH` short form), `-u`/`--uniform-spacing`, and `-s`/`--split-only` (including its exact-spacing-preserved-when-line-already-fits behavior) all work as documented above |
| `split` | partial | `-n`/`--number=CHUNKS` (all its `N`/`K/N`/`l/N`/`l/K/N`/`r/N`/`r/K/N` forms), `--filter=COMMAND`, `-u`/`--unbuffered`, `-e`/`--elide-empty-files`, and `--verbose` are not implemented (the last three are accepted but silently ignored); `--numeric-suffixes=FROM`/`--hex-suffixes=FROM` always start at 0, a custom `FROM` value is not supported; decimal (power-of-1000) `SIZE` suffixes like `KB`/`MB` are not distinguished from the binary (power-of-1024) forms; `-l`/`--lines`, `-b`/`--bytes`, `-C`/`--line-bytes`, `-a`/`--suffix-length`, `-d`, `-x`, `--additional-suffix`, `-t`/`--separator`, and suffix-exhaustion detection all work |
| `join` | partial | `--check-order`/`--nocheck-order` are accepted but input order is never verified (no disorder warning is ever printed, unlike GNU's default behavior); `-o auto` is not implemented, only an explicit `FILENUM.FIELD`/`0` list; collation for comparing join fields is always byte-order, never locale-aware; `-a`, `-v`, `-e`, `-i`, `-j`, `-o`, `-t` (attached and separate forms), `-1`/`-2`, `--header`, and `-z` all work |
| `sort` | partial | collation is always byte-order (as if `LC_ALL=C`), never locale-aware; `-d`/`--dictionary-order`, `-i`/`--ignore-nonprinting`, `-M`/`--month-sort`, `-h`/`--human-numeric-sort`, `-V`/`--version-sort`, `-R`/`--random-sort`, `--random-source`, `--files0-from`, `--compress-program`, `-S`/`--buffer-size`, `-T`/`--temporary-directory`, `--parallel`, `--batch-size`, and `--debug` are not implemented; `-g`/`--general-numeric-sort` uses the same decimal-digit comparison as `-n` rather than true floating-point/scientific-notation parsing; `-m`/`--merge` is implemented as a full re-sort rather than a true merge, which only matches GNU exactly when every input file is already individually sorted; a `-k` key's per-key `OPTS` letters replace rather than selectively override the global ordering flags for that key, and a key's character-position start/end are clamped to the field's own boundary rather than being allowed to run into subsequent text; `-r`, `-u`, `-n`, `-f`, `-b`, `-c`/`-C`, `-k`/`--key`, `-t`/`--field-separator`, `-o`/`--output`, `-s`/`--stable`, and `-z`/`--zero-terminated` all work |
| `date` | partial | `-d`/`--date=STRING` only parses `@EPOCH`, `now`/`today`, and `YYYY-MM-DD[ HH:MM[:SS]]`/`YYYY-MM-DDTHH:MM[:SS]`; GNU's full natural-language date parser (relative phrases like "next friday", "3 days ago", weekday/month names, locale formats) is not implemented; `-s`/`--set` and the `MMDDhhmm[[CC]YY][.ss]` positional set-time form are not implemented (this reimplementation never modifies the system clock); `-f`/`--file`, `--debug`, and `--resolution` are not implemented; `%C`/`%g`/`%G`/`%U`/`%V`/`%W` format specifiers (century and ISO/US week numbers) are not implemented; parsing a wall-clock `-d` string resolves the UTC offset from a single approximation pass (assuming the given fields are already UTC) rather than iterating to a fixed point, so a `-d` string that falls in the hour(s) around a DST transition in a non-UTC zone can resolve to the wrong offset; `-u`, `-R`, `-I`/`--iso-8601[=FMT]`, `--rfc-3339=FMT`, `-r`/`--reference=FILE`, and the remaining `+FORMAT` specifiers all work |

“Complete” means the documented option surface is implemented and covered by
the current parity suite. It does not claim locale, filesystem, or kernel
behaviors that cannot be exercised on the development host.

## Utilities not implemented yet

The installed GNU 9.11 suite contains 103 command names. The initial
filesystem-tool pass is complete; implementation now proceeds through
text/data tools, checksums, and the remaining system/account utilities.
