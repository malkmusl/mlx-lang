"""HarfBuzz's shaping tests (test/shape/data/*/tests/*.tests of its source)
run as its run-tests.py runs them, with hb-shape.c built against the
system's HarfBuzz (REF) and against this library (MINE) in place of
hb-shape. Every case gives the expected glyphs with MINE, but those of
Apple's AAT tables (not implemented); and MINE gives what REF gives for
every case but those. Prints the cases that fail and the counts; exits 1
when one fails.

    run_shape_tests.py REF MINE TESTFILE..."""
import os
import subprocess
import sys

# The tests of Apple's AAT tables (morx, kerx, trak, ankr), which the
# library does not implement.
AAT = ('aat-', 'MORX-', 'KERX-', 'TRAK-')

ref, mine = sys.argv[1], sys.argv[2]
cases = []
skipped = 0
for filename in sys.argv[3:]:
    for line in open(filename, encoding='utf8'):
        if line.startswith('#'):
            continue
        line = line.strip()
        if not line:
            continue
        fontfile, options, unicodes, expected = line.split(';')
        options = options.split()
        if fontfile.startswith('/') or fontfile.startswith('"/'):
            fontfile = fontfile.split('@')[0]
            if not os.path.exists(fontfile):
                skipped += 1
                continue
        else:
            fontfile = os.path.normpath(os.path.join(os.path.dirname(filename), fontfile))
        if any(o in options for o in ('--shaper=coretext', '--shaper=directwrite', '--shaper=uniscribe')):
            skipped += 1
            continue
        extra = ['--shaper=ot']
        if expected != '*':
            extra += ['--verify', '--unsafe-to-concat']
        # (Both font functions unless the case names one, as run-tests.py.)
        funcs = ['ot'] if '--font-funcs=ot' in options else ['ft', 'ot']
        for f in funcs:
            args = [fontfile, '--font-funcs=' + f] + extra + ['--unicodes', unicodes] + options
            cases.append((os.path.basename(filename), args, expected))

lines = ['\t'.join(c[1]) + '\n' for c in cases]


def run(binary):
    """The output line of each case; a case that crashes the program gives
    '<crash>' and the rest run again in a new process."""
    out = []
    while len(out) < len(lines):
        jobs = ''.join(lines[len(out):]).encode('utf8')
        p = subprocess.run([binary], input=jobs, stdout=subprocess.PIPE, timeout=3000)
        got = p.stdout.decode('utf8', 'replace').split('\n')
        if got and got[-1] == '':
            got.pop()
        out += got
        if len(out) < len(lines):
            out.append('<crash %d>' % p.returncode)
    return out


ref_out = run(ref)
mine_out = run(mine)
same = expected_ok = aat = failed = 0
for i, (filename, args, expected) in enumerate(cases):
    r, m = ref_out[i].strip(), mine_out[i].strip()
    if filename.startswith(AAT):
        aat += 1
        continue
    good = m == r and (expected == '*' or m == expected)
    same += m == r
    expected_ok += expected == '*' or m == expected
    if not good:
        failed += 1
        if failed <= 30:
            print('FAIL', filename, ' '.join(args[1:]))
            print('  system:   ' + r[:300])
            print('  library:  ' + m[:300])
            print('  expected: ' + expected[:300])
print('%d cases, %d as expected and as the system\'s HarfBuzz, %d of AAT left out, %d skipped (fonts not here, other shapers)'
      % (len(cases), len(cases) - aat - failed, aat, skipped))
sys.exit(1 if failed else 0)
