# Every coreutils program, as brixOS runs it and as the host does
# (tools/check_brix.sh compares the two): what each prints between
# "@@ command" and "@@ status N". Nothing here may depend on the machine:
# not the directory (both work in one named work), the user, the CPUs.
# Standard input is the null device (no program waits for a terminal):
# $N, /dev/null on the host, /devices/null in brixOS (its programs see
# brixOS's own paths).
if [ -n "$T" ]; then N=/dev/null; else N=/devices/null; fi
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
echo "@@ logname"; logname > $N 2>&1; echo "@@ ran"
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
r tr a-z A-Z < f
r env true
r truncate -s 3 f
r cat f
r uname -m
r unexpand -a spaces
r uniq s1
r unlink hard
r wc n
echo "@@ whoami"; whoami > $N; echo "@@ status $?"
echo "@@ yes | head -n 2"; yes | head -n 2; echo "@@ status $?"
r ls -1

# A FIFO opened to read and write at once (it waits for no one).
echo "@@ fifo read-write"; exec 3<> fifo; echo both >&3; read line <&3; echo "got $line"; exec 3>&-; echo "@@ status $?"

# The programs again, with more of their options.
seq 100000 > big
r wc -l big
r sort -rn -o sorted big
r head -n 3 sorted
r tail -c 13 big
r head -c 20 big
r tail -n +99998 big
r sort -t ' ' -k 2 -n j1
r uniq -c s1
r uniq -d s1
echo "@@ cut | tail"; cut -c 1-3 big | tail -n 2; echo "@@ status $?"
r tr -d a < f
r tr -s '\n' < s1
r cat -n -A tabs
r nl -b a s1
r fold -w 1 -s j1
r paste -s -d , s1
r join -1 1 -2 1 -o 1.2,2.2 j1 j2
r comm -12 s1 s2
r expand -t 4 tabs
r unexpand -t 4 spaces
r seq -w 8 10
r seq -s , 1 5
r seq 1 0.5 3
r expr length abcdef
r expr substr abcdef 2 3
r expr 7 '*' 6
r env test -d .
r env test -s big
r env [ abc = abc ]
r basename -s .txt a.txt b.txt
r dirname a/b/c /x
r date -u -d @1000000 '+%F %T %A %j'
r touch -d @86400 old
r stat -c '%Y %s' old
r truncate -s 10 old
r stat -c %s old
r truncate -s +5 old
r stat -c %s old
r mkdir -p deep/a/b/c
r mkdir -m 700 private
r stat -c %a private
r cp -r deep copy
r ls -R -1 copy
r mv copy moved
r ls -a -1 moved
r rmdir -p moved/a/b/c
r rm -r deep private old
r ln -s f link1
r ln -sf j1 link1
r readlink link1
echo "@@ readlink -f link1"; basename "$(readlink -f link1)"; echo "@@ status $?"
echo "@@ realpath link1"; basename "$(realpath link1)"; echo "@@ status $?"
r realpath -s --relative-to=. link1
r mkdir -p sub/inner
r ln -s ../../f sub/inner/up
r cat sub/inner/up
echo "@@ readlink -f sub/inner/up"; basename "$(readlink -f sub/inner/up)"; echo "@@ status $?"
r rm -r sub link1
r mkfifo -m 600 fifo2
r stat -c '%F %a' fifo2
r rm fifo2
base64 f > enc
r base64 -d enc
basenc --base32 f > enc32
r basenc --base32 -d enc32
md5sum f > sums
r md5sum -c sums
sha256sum f > sums
r sha256sum -c sums
r cksum -a sha1 f
r sum -s f
r split -b 7 n piece
r ls -1 piece*
echo "@@ cat piece* | wc"; cat piece* | wc -l; echo "@@ status $?"
r rm piece*
r csplit -f part n '/5/'
r cat part00
r rm part00 part01
r pr -T -o 2 n
r pr -2 -T -w 20 n
r pr -3 -a -t -s, n
r pr -m -t n j1
r tac -s , enc
r tee -a t1 < f
r cat t1
r chmod -R go-r t1
r stat -c %a t1
r shuf -i 1-5 -n 0
echo "@@ shuf -i | sort"; shuf -i 1-5 | sort -n; echo "@@ status $?"
echo "@@ env -i A=1 printenv A"; env -i A=1 PATH="$PATH" printenv A; echo "@@ status $?"
r env -u WORD printenv WORD
r printenv HOME
r sleep 0.1
echo "@@ yes | head"; yes ab | head -n 3; echo "@@ status $?"
r link f f2
r stat -c %h f
r unlink f2
r stat -c %h f
echo "@@ done"
