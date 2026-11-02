# Every coreutils program once, as brixOS runs it and as the host does
# (tools/check_brix.sh compares the two): what each prints between
# "@@ command" and "@@ status N". Nothing here may depend on the machine:
# not the directory (both work in one named work), the user, the CPUs.
umask 022
cd ${T:-/temporary} && mkdir work && cd work
printf 'b\na\nc\na\n' > f; seq 10 > n; printf 'x 1\ny 2\n' > j1; printf 'x A\ny B\n' > j2
printf 'a\tb\n' > tabs; printf 'a       b\n' > spaces; printf 'a\nd\n' > s2
r() { echo "@@ $*"; "$@"; echo "@@ status $?"; }
r env [ 1 -lt 2 ]
r arch
r b2sum f
r base32 f
r base64 f
r basename /a/b.c .c
r basenc --base16 f
r cat f
r chgrp "$(stat -c %g f)" f
r chmod 640 f
r chown "$(stat -c %u f):$(stat -c %g f)" f
r cksum f
r sort -o s1 f
r comm s1 s2
r cp f g
r csplit n 5
r cut -d ' ' -f 2 j1
r date -u -d @0 +%Y-%m-%d
r dirname /a/b
r env echo from env
r expand tabs
r expr 2 + 3
r env false
r fmt -w 5 f
r fold -w 2 j1
r head -n 2 n
r join j1 j2
r link f hard
r ln -s f soft
echo "@@ logname"; logname > /dev/null 2>&1; echo "@@ ran"
r ls -1
r md5sum f
r mkdir dir
r mkfifo fifo
r mv g moved
r nl f
echo "@@ nproc"; echo $(( $(nproc) >= 1 )); echo "@@ status $?"
r paste j1 j2
r pr -T f
export WORD=exported
r printenv WORD
echo "@@ pwd"; basename "$(env pwd)"; echo "@@ status $?"
r readlink soft
r cat soft
r stat -c %F soft
r stat -L -c %F soft
r ln -s nowhere dangling
r readlink dangling
r cat dangling
r mkdir -p real/inner
r ln -s real lnk
r ls lnk/
r ln -s loop1 loop2
r ln -s loop2 loop1
r cat loop1
r rm loop1 loop2 lnk dangling
r rm -r real
echo "@@ fifo"; echo through fifo > fifo & cat fifo; wait; echo "@@ status $?"
r stat -c %F fifo
echo "@@ realpath f"; basename "$(realpath f)"; echo "@@ status $?"
r rm moved
r rmdir dir
r seq 3
r sha1sum f
r sha224sum f
r sha256sum f
r sha384sum f
r sha512sum f
echo "@@ shuf f | sort"; shuf f | sort; echo "@@ status $?"
r sleep 0
r sort -u f
r split -l 4 n part
r ls -1
r stat -c '%s %a %h %F' f
r sum f
r sync
r tac f
r tail -n 2 n
echo "@@ tee"; echo teed | tee t1; cat t1; echo "@@ status $?"
r env test -f f
r touch touched
r tr a-z A-Z f
r env true
r truncate -s 3 f
r cat f
r uname -m
r unexpand -a spaces
r uniq s1
r unlink hard
r wc n
echo "@@ whoami"; whoami > /dev/null; echo "@@ status $?"
echo "@@ yes | head -n 2"; yes | head -n 2; echo "@@ status $?"
r ls -1
echo "@@ done"
