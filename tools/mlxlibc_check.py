#!/usr/bin/env python3
"""mlxlibc against glibc: the same calls through ctypes into both libraries
must answer the same (tools/check_mlxlibc.sh runs this).

    mlxlibc_check.py PATH/libmlxc.so [section ...]
"""
import ctypes, os, sys, tempfile, threading, time

mlx = ctypes.CDLL(sys.argv[1])
glibc = ctypes.CDLL("libc.so.6")
sections = sys.argv[2:]
failures = []
checked = 0

cp, cs, ci, cu, cv, cl, cul, cd = ctypes.c_char_p, ctypes.c_size_t, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_long, ctypes.c_ulong, ctypes.c_double


def check(name, ours, theirs, note=""):
    global checked
    checked += 1
    if ours != theirs:
        failures.append(f"{name}{note}: mlx={ours!r} glibc={theirs!r}")


def fn(lib, name, res, args):
    f = getattr(lib, name)
    f.restype = res
    f.argtypes = args
    return f


def both(name, res, args):
    return fn(mlx, name, res, args), fn(glibc, name, res, args)


def wanted(section):
    return not sections or section in sections


WORDS = [b"", b"a", b"hello", b"hello world", b"Hello", b"HELLO", b"abc\x00def", b"xyz", b"hello\x00", b"  spaced ", b"\xff\x80\x01", b"a,b,,c"]

if wanted("string"):
    for name, res, args in [("strlen", cs, [cp]), ("strcmp", ci, [cp, cp]), ("strcasecmp", ci, [cp, cp]), ("strstr", cv, [cp, cp]), ("strcasestr", cv, [cp, cp]),
                            ("strchr", cv, [cp, ci]), ("strrchr", cv, [cp, ci]), ("strchrnul", cv, [cp, ci]), ("strspn", cs, [cp, cp]), ("strcspn", cs, [cp, cp]), ("strpbrk", cv, [cp, cp]),
                            ("strncmp", ci, [cp, cp, cs]), ("strncasecmp", ci, [cp, cp, cs]), ("memcmp", ci, [cp, cp, cs]), ("strnlen", cs, [cp, cs])]:
        m, g = both(name, res, args)
        for a in WORDS:
            for b in WORDS:
                ab = ctypes.create_string_buffer(a, 32)
                bb = ctypes.create_string_buffer(b, 32)
                if len(args) == 1: argsets = [(ab,)]
                elif args[1] is ci: argsets = [(ab, c) for c in (0, 97, 108, 0x6f, 255, 0x1ff)]
                elif args[1] is cs: argsets = [(ab, n) for n in (0, 1, 3, 5, 32)]
                elif len(args) == 3: argsets = [(ab, bb, n) for n in (0, 1, 3, 5, 32)]
                else: argsets = [(ab, bb)]
                for argset in argsets:
                    r1, r2 = m(*argset), g(*argset)
                    if res is cv:
                        r1 = None if r1 is None else r1 - ctypes.addressof(argset[0])
                        r2 = None if r2 is None else r2 - ctypes.addressof(argset[0])
                    elif res is ci:
                        r1, r2 = (r1 > 0) - (r1 < 0), (r2 > 0) - (r2 < 0)
                    check(name, r1, r2, f"{argset[1:] if len(argset) > 1 else ''} on {a!r},{b!r}")
    for name in ["strcpy", "strcat", "stpcpy"]:
        m, g = both(name, cv, [cv, cp])
        for a in WORDS:
            b1 = ctypes.create_string_buffer(b"pre", 64)
            b2 = ctypes.create_string_buffer(b"pre", 64)
            r1, r2 = m(b1, a), g(b2, a)
            check(name, b1.raw, b2.raw, repr(a))
            check(name + " result", r1 - ctypes.addressof(b1), r2 - ctypes.addressof(b2), repr(a))
    for name in ["strncpy", "strncat", "strlcpy", "strlcat"]:
        m, g = both(name, cv if name.startswith("strn") else cs, [cv, cp, cs])
        for a in WORDS:
            for n in (0, 2, 5, 20):
                b1 = ctypes.create_string_buffer(b"pre", 64)
                b2 = ctypes.create_string_buffer(b"pre", 64)
                r1, r2 = m(b1, a, n), g(b2, a, n)
                check(name, b1.raw, b2.raw, f"{a!r},{n}")
                if name.startswith("strl"): check(name + " result", r1, r2, f"{a!r},{n}")
    m, g = both("memchr", cv, [cv, ci, cs])
    buf = ctypes.create_string_buffer(b"abcabc\x00xyz", 16)
    for c in (97, 99, 0, 120, 0x161):
        for n in (0, 3, 7, 11):
            check("memchr", m(buf, c, n), g(buf, c, n), f"{c},{n}")
    m, g = both("memrchr", cv, [cv, ci, cs])
    for c in (97, 99, 0, 120):
        for n in (0, 3, 7, 11):
            check("memrchr", m(buf, c, n), g(buf, c, n), f"{c},{n}")
    mm = fn(mlx, "memmove", cv, [cv, cv, cs])
    b = ctypes.create_string_buffer(b"0123456789", 16)
    mm(ctypes.addressof(b) + 2, ctypes.addressof(b), 6)
    check("memmove forward", b.raw[:10], b"0101234589")
    b = ctypes.create_string_buffer(b"0123456789", 16)
    mm(ctypes.addressof(b), ctypes.addressof(b) + 2, 6)
    check("memmove back", b.raw[:10], b"2345676789")
    ms = fn(mlx, "memset", cv, [cv, ci, cs])
    b = ctypes.create_string_buffer(16)
    ms(b, 0x141, 5)
    check("memset", b.raw[:6], b"AAAAA\0")
    for name in ["strtok_r"]:
        for lib in (mlx, glibc):
            tok = fn(lib, name, cv, [cv, cp, cv])
            save = cv()
            s = ctypes.create_string_buffer(b",a,b,,c,", 16)
            parts = []
            p = tok(s, b",", ctypes.byref(save))
            while p:
                parts.append(ctypes.string_at(p))
                p = tok(None, b",", ctypes.byref(save))
            if lib is mlx: ours = parts
            else: check("strtok_r", ours, parts)
    for lib in (mlx, glibc):
        sep = fn(lib, "strsep", cv, [cv, cp])
        s = ctypes.create_string_buffer(b"a,b,,c", 16)
        cursor = cv(ctypes.addressof(s))
        parts = []
        p = sep(ctypes.byref(cursor), b",")
        while p:
            parts.append(ctypes.string_at(p))
            p = sep(ctypes.byref(cursor), b",")
        if lib is mlx: ours = parts
        else: check("strsep", ours, parts)
    m, g = both("strerror", cp, [ci])
    for e in list(range(0, 141)) + [-1, 1000]:
        check("strerror", m(e), g(e), f"({e})")
    m, g = both("strdup", cv, [cp])
    for a in WORDS:
        check("strdup", ctypes.string_at(m(a)), ctypes.string_at(g(a)), repr(a))
    m, g = both("strndup", cv, [cp, cs])
    for a in WORDS:
        for n in (0, 2, 40):
            check("strndup", ctypes.string_at(m(a, n)), ctypes.string_at(g(a, n)), f"{a!r},{n}")
    for name in ["ffs"]:
        m, g = both(name, ci, [ci])
        for v in (0, 1, 2, 12, -1, -8, 0x40000000):
            check(name, m(v), g(v), f"({v})")
    m, g = both("ffsl", ci, [cl])
    for v in (0, 1, 1 << 40, -1):
        check("ffsl", m(v), g(v), f"({v})")

if wanted("ctype"):
    for name in ["isalpha", "isdigit", "isalnum", "isupper", "islower", "isspace", "isblank", "iscntrl", "isprint", "isgraph", "ispunct", "isxdigit", "isascii", "tolower", "toupper"]:
        m, g = both(name, ci, [ci])
        for c in range(-1, 256):
            r1, r2 = m(c), g(c)
            if name.startswith("is"): r1, r2 = bool(r1), bool(r2)
            check(name, r1, r2, f"({c})")
    P16 = ctypes.POINTER(ctypes.POINTER(ctypes.c_uint16))
    P32 = ctypes.POINTER(ctypes.POINTER(ctypes.c_int32))
    t1, t2 = fn(mlx, "__ctype_b_loc", P16, [])(), fn(glibc, "__ctype_b_loc", P16, [])()
    for c in range(-128, 256): check("__ctype_b_loc", t1[0][c], t2[0][c], f"[{c}]")
    for name in ["__ctype_tolower_loc", "__ctype_toupper_loc"]:
        t1, t2 = fn(mlx, name, P32, [])(), fn(glibc, name, P32, [])()
        for c in range(-128, 256): check(name, t1[0][c], t2[0][c], f"[{c}]")

if wanted("malloc"):
    malloc = fn(mlx, "malloc", cv, [cs]); free = fn(mlx, "free", None, [cv]); calloc = fn(mlx, "calloc", cv, [cs, cs])
    realloc = fn(mlx, "realloc", cv, [cv, cs]); pma = fn(mlx, "posix_memalign", ci, [ctypes.POINTER(cv), cs, cs]); mus = fn(mlx, "malloc_usable_size", cs, [cv])
    blocks = []
    for size in [0, 1, 7, 16, 100, 1000, 40000, 300000, 5000000]:
        p = malloc(size)
        check("malloc aligned", p is not None and p % 16 == 0, True, f"({size})")
        check("malloc_usable_size", mus(p) >= max(size, 1), True, f"({size})")
        ctypes.memset(p, 0xAB, size)
        blocks.append(p)
    for p in blocks: free(p)
    p = calloc(10, 1000)
    check("calloc zeroed", ctypes.string_at(p, 10000), b"\0" * 10000)
    free(p)
    p = malloc(50)
    ctypes.memmove(p, b"x" * 50, 50)
    q = realloc(p, 5000)
    check("realloc keeps", ctypes.string_at(q, 50), b"x" * 50)
    q = realloc(q, 7)
    check("realloc shrink keeps", ctypes.string_at(q, 7), b"x" * 7)
    free(q)
    for align in (16, 64, 4096, 65536):
        out = cv()
        check("posix_memalign", pma(ctypes.byref(out), align, 1000), 0, f"({align})")
        check("posix_memalign aligned", out.value % align, 0, f"({align})")
        ctypes.memset(out.value, 1, 1000)
        free(out.value)
    check("posix_memalign EINVAL", pma(ctypes.byref(out), 24, 10), 22)
    def resident(): return int(open('/proc/self/statm').read().split()[1])
    before = resident()
    for i in range(20000):
        p = malloc(100 + (i % 500) * 8)
        free(p)
    check("malloc/free loop stays flat", resident() - before < 200, True, f" grew {resident() - before} pages")
    el = fn(mlx, "__errno_location", ctypes.POINTER(ci), [])
    el()[0] = 0
    check("malloc huge fails", malloc(1 << 62), None)
    check("ENOMEM", el()[0], 12)


if wanted("stdlib"):
    for name, res in [("strtol", cl), ("strtoul", cul), ("strtoll", ctypes.c_longlong), ("strtoull", ctypes.c_ulonglong)]:
        m, g = both(name, res, [cp, ctypes.POINTER(cp), ci])
        el1 = fn(mlx, "__errno_location", ctypes.POINTER(ci), [])
        el2 = fn(glibc, "__errno_location", ctypes.POINTER(ci), [])
        for text in [b"0", b"42", b"-42", b"  +17x", b"0x1fZ", b"0X", b"077", b"08", b"9223372036854775807", b"9223372036854775808", b"-9223372036854775808", b"-9223372036854775809",
                     b"18446744073709551615", b"18446744073709551616", b"-1", b"abc", b"", b"   ", b"-", b"+-3", b"1e5", b"z", b"0b101", b"ff", b"FF"]:
            for base in (0, 2, 8, 10, 16, 36):
                e1, e2 = cp(), cp()
                el1()[0] = 0; el2()[0] = 0
                buf = ctypes.create_string_buffer(text, 40)
                r1 = m(buf, ctypes.byref(e1), base)
                r2 = g(buf, ctypes.byref(e2), base)
                check(name, (r1, el1()[0]), (r2, el2()[0]), f"({text!r}, {base})")
                check(name + " end", ctypes.cast(e1, cv).value - ctypes.addressof(buf), ctypes.cast(e2, cv).value - ctypes.addressof(buf), f"({text!r}, {base})")
    for name, res in [("atoi", ci), ("atol", cl), ("atoll", ctypes.c_longlong)]:
        m, g = both(name, res, [cp])
        for text in [b"12", b"-7", b" 99 bottles", b"", b"x", b"2147483648", b"-2147483649", b"99999999999999999999"]:
            check(name, m(text), g(text), repr(text))
    m, g = both("strtod", cd, [cp, ctypes.POINTER(cp)])
    import math, struct
    for text in [b"0", b"1", b"-1", b"3.14", b"-3.14159", b"1e10", b"1E-10", b".5", b"5.", b"1e308", b"1e309", b"-1e309", b"1e-320", b"2.5e-308", b"123456789012345678", b"0.1", b"0.3", b"1.7976931348623157e308",
                 b"inf", b"-Infinity", b"nan", b"NaN(123)", b"  12abc", b"abc", b"", b"-", b"1e", b"0x1p3", b"0x1.8p1", b"0xA", b"1.5e+2", b"4.9406564584124654e-324", b"6.02214076e23", b"1234.5678e-3"]:
        e1, e2 = cp(), cp()
        buf = ctypes.create_string_buffer(text, 40)
        r1, r2 = m(buf, ctypes.byref(e1)), g(buf, ctypes.byref(e2))
        same = (math.isnan(r1) and math.isnan(r2)) or struct.pack("d", r1) == struct.pack("d", r2)
        if not same and r1 and r2 and math.isfinite(r1) and math.isfinite(r2):
            # Beyond one rounding step a last-bit difference is allowed.
            same = abs(struct.unpack("q", struct.pack("d", r1))[0] - struct.unpack("q", struct.pack("d", r2))[0]) <= 2
        check("strtod", True, same, f"({text!r}) mlx={r1!r} glibc={r2!r}")
        check("strtod end", ctypes.cast(e1, cv).value - ctypes.addressof(buf), ctypes.cast(e2, cv).value - ctypes.addressof(buf), repr(text))
    m, g = both("abs", ci, [ci])
    for v in (0, 5, -5, -2147483647): check("abs", m(v), g(v), f"({v})")
    m, g = both("labs", cl, [cl])
    for v in (0, 5, -5, -(2**62)): check("labs", m(v), g(v), f"({v})")
    m, g = both("div", ctypes.c_uint64, [ci, ci])
    for a, b in [(7, 2), (-7, 2), (7, -2), (-7, -2), (0, 3), (2147483647, 7)]:
        check("div", m(a, b), g(a, b), f"({a}, {b})")
    CMP = ctypes.CFUNCTYPE(ci, cv, cv)
    @CMP
    def compare_ints(a, b):
        x, y = ctypes.cast(a, ctypes.POINTER(ci))[0], ctypes.cast(b, ctypes.POINTER(ci))[0]
        return (x > y) - (x < y)
    import random as pyrandom
    pyrandom.seed(7)
    for count in (0, 1, 2, 5, 100, 1000):
        values = [pyrandom.randint(-1000, 1000) for _ in range(count)]
        arr = (ci * max(count, 1))(*values) if count else (ci * 1)()
        fn(mlx, "qsort", None, [cv, cs, cs, CMP])(arr, count, 4, compare_ints)
        check("qsort", list(arr)[:count], sorted(values), f"({count})")
        bs = fn(mlx, "bsearch", cv, [cv, cv, cs, cs, CMP])
        for key in (values[0] if values else 0, 5000, -5000):
            k = ci(key)
            found = bs(ctypes.byref(k), arr, count, 4, compare_ints)
            check("bsearch", found is not None, key in values, f"({count}, {key})")
    # The environment: this process's, read from /proc/self/environ.
    getenv = fn(mlx, "getenv", cp, [cp])
    for name in ["HOME", "PATH", "NO_SUCH_VARIABLE_HERE", ""]:
        check("getenv", getenv(name.encode()), os.environ.get(name).encode() if os.environ.get(name) is not None else None, f"({name})")
    setenv = fn(mlx, "setenv", ci, [cp, cp, ci]); unsetenv = fn(mlx, "unsetenv", ci, [cp]); putenv = fn(mlx, "putenv", ci, [cp])
    check("setenv", setenv(b"MLX_CHECK", b"one", 1), 0); check("setenv value", getenv(b"MLX_CHECK"), b"one")
    check("setenv keeps", setenv(b"MLX_CHECK", b"two", 0), 0); check("setenv kept", getenv(b"MLX_CHECK"), b"one")
    check("setenv overwrites", setenv(b"MLX_CHECK", b"two", 1), 0); check("setenv overwrote", getenv(b"MLX_CHECK"), b"two")
    kept = ctypes.create_string_buffer(b"MLX_PUT=three")
    check("putenv", putenv(kept), 0); check("putenv value", getenv(b"MLX_PUT"), b"three")
    check("unsetenv", unsetenv(b"MLX_CHECK"), 0); check("unsetenv gone", getenv(b"MLX_CHECK"), None)
    check("getenv still", getenv(b"HOME"), os.environ["HOME"].encode())
    check("setenv EINVAL", setenv(b"A=B", b"x", 1), -1)
    # rand: glibc's generator, the same numbers for the same seed.
    for seed in (1, 42, 123456789):
        fn(mlx, "srand", None, [ctypes.c_uint])(seed); fn(glibc, "srand", None, [ctypes.c_uint])(seed)
        check("rand", [fn(mlx, "rand", ci, [])() for _ in range(50)], [fn(glibc, "rand", ci, [])() for _ in range(50)], f"(seed {seed})")
    check("rand unseeded", [fn(mlx, "random", cl, [])() for _ in range(3)] != [0, 0, 0], True)
    for seed in (1, 99):
        s1, s2 = ctypes.c_uint(seed), ctypes.c_uint(seed)
        check("rand_r", [fn(mlx, "rand_r", ci, [ctypes.POINTER(ctypes.c_uint)])(ctypes.byref(s1)) for _ in range(20)], [fn(glibc, "rand_r", ci, [ctypes.POINTER(ctypes.c_uint)])(ctypes.byref(s2)) for _ in range(20)], f"(seed {seed})")
    system = fn(mlx, "system", ci, [cp])
    check("system true", system(b"true"), 0)
    check("system exit 3", system(b"exit 3"), 3 << 8)
    check("system writes", system(b"echo mlxlibc-system > " + tempfile.gettempdir().encode() + b"/mlxlibc-system.txt"), 0)
    check("system wrote", open(tempfile.gettempdir() + "/mlxlibc-system.txt").read(), "mlxlibc-system\n")

if wanted("unistd"):
    el = fn(mlx, "__errno_location", ctypes.POINTER(ci), [])
    check("getpid", fn(mlx, "getpid", ci, [])(), os.getpid())
    check("getppid", fn(mlx, "getppid", ci, [])(), os.getppid())
    check("getuid", fn(mlx, "getuid", ctypes.c_uint, [])(), os.getuid())
    check("geteuid", fn(mlx, "geteuid", ctypes.c_uint, [])(), os.geteuid())
    check("gettid", fn(mlx, "gettid", ci, [])(), threading.get_native_id())
    work = tempfile.mkdtemp(prefix="mlxlibc-")
    path = (work + "/file.txt").encode()
    opn = fn(mlx, "open", ci, [cp, ci, ctypes.c_uint]); wr = fn(mlx, "write", ctypes.c_ssize_t, [ci, cv, cs]); rd = fn(mlx, "read", ctypes.c_ssize_t, [ci, cv, cs]); cl_ = fn(mlx, "close", ci, [ci]); lsk = fn(mlx, "lseek", cl, [ci, cl, ci])
    fd = opn(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
    check("open creates", fd >= 0, True)
    check("write", wr(fd, b"hello file\n", 11), 11)
    check("close", cl_(fd), 0)
    check("file content", open(path).read(), "hello file\n")
    fd = opn(path, os.O_RDONLY, 0)
    buf = ctypes.create_string_buffer(32)
    check("lseek", lsk(fd, 6, 0), 6)
    check("read", rd(fd, buf, 32), 5); check("read content", buf.raw[:5], b"file\n")
    check("read eof", rd(fd, buf, 32), 0)
    check("lseek end", lsk(fd, 0, 2), 11)
    cl_(fd)
    el()[0] = 0
    check("open missing", opn(b"/no/such/file", os.O_RDONLY, 0), -1); check("ENOENT", el()[0], 2)
    st = ctypes.create_string_buffer(144); st2 = ctypes.create_string_buffer(144)
    check("stat", fn(mlx, "stat", ci, [cp, cv])(path, st), 0); fn(glibc, "stat", ci, [cp, cv])(path, st2)
    check("stat struct", st.raw, st2.raw)
    check("stat size", ctypes.cast(ctypes.addressof(st) + 48, ctypes.POINTER(cl))[0], 11)
    check("access", fn(mlx, "access", ci, [cp, ci])(path, os.R_OK), 0)
    check("access missing", fn(mlx, "access", ci, [cp, ci])(b"/no/such", os.F_OK), -1)
    cwd = ctypes.create_string_buffer(4096)
    check("getcwd", fn(mlx, "getcwd", cv, [cv, cs])(cwd, 4096) is not None and cwd.value.decode() == os.getcwd(), True)
    got = fn(mlx, "getcwd", cp, [cv, cs])(None, 0)
    check("getcwd allocates", got, os.getcwd().encode())
    check("mkdir", fn(mlx, "mkdir", ci, [cp, ctypes.c_uint])((work + "/sub").encode(), 0o755), 0)
    check("mkdir exists", fn(mlx, "mkdir", ci, [cp, ctypes.c_uint])((work + "/sub").encode(), 0o755), -1); check("EEXIST", el()[0], 17)
    check("symlink", fn(mlx, "symlink", ci, [cp, cp])(b"file.txt", (work + "/link").encode()), 0)
    link = ctypes.create_string_buffer(64)
    check("readlink", fn(mlx, "readlink", ctypes.c_ssize_t, [cp, cv, cs])((work + "/link").encode(), link, 64), 8); check("readlink target", link.raw[:8], b"file.txt")
    check("rename", fn(mlx, "rename", ci, [cp, cp])((work + "/link").encode(), (work + "/link2").encode()), 0)
    check("renamed", os.path.islink(work + "/link2"), True)
    check("listdir", sorted(os.listdir(work)), ["file.txt", "link2", "sub"])
    # opendir/readdir
    names = []
    opendir = fn(mlx, "opendir", cv, [cp]); readdir = fn(mlx, "readdir", cv, [cv]); closedir = fn(mlx, "closedir", ci, [cv])
    d = opendir(work.encode())
    check("opendir", d is not None, True)
    while True:
        e = readdir(d)
        if not e: break
        names.append(ctypes.string_at(e + 19))
    check("readdir", sorted(names), sorted([b".", b"..", b"file.txt", b"link2", b"sub"]))
    check("closedir", closedir(d), 0)
    check("opendir missing", opendir(b"/no/such/dir"), None)
    check("unlink", fn(mlx, "unlink", ci, [cp])((work + "/link2").encode()), 0)
    check("rmdir", fn(mlx, "rmdir", ci, [cp])((work + "/sub").encode()), 0)
    check("unlink file", fn(mlx, "unlink", ci, [cp])(path), 0)
    check("rmdir work", fn(mlx, "rmdir", ci, [cp])(work.encode()), 0)
    fds = (ci * 2)()
    check("pipe", fn(mlx, "pipe", ci, [cv])(fds), 0)
    check("pipe write", wr(fds[1], b"abc", 3), 3)
    pfd = ctypes.create_string_buffer(8)
    ctypes.cast(pfd, ctypes.POINTER(ci))[0] = fds[0]; ctypes.cast(pfd, ctypes.POINTER(ctypes.c_short))[2] = 1
    check("poll", fn(mlx, "poll", ci, [cv, ctypes.c_ulong, ci])(pfd, 1, 0), 1)
    check("poll revents", ctypes.cast(pfd, ctypes.POINTER(ctypes.c_short))[3] & 1, 1)
    check("pipe read", rd(fds[0], buf, 32), 3)
    check("isatty pipe", fn(mlx, "isatty", ci, [ci])(fds[0]), 0); check("ENOTTY", el()[0], 25)
    dup = fn(mlx, "dup", ci, [ci])(fds[0]); check("dup", dup > fds[1], True); cl_(dup)
    cl_(fds[0]); cl_(fds[1])
    efd = fn(mlx, "eventfd", ci, [ctypes.c_uint, ci])(5, 0)
    val = ctypes.c_uint64()
    check("eventfd_read", fn(mlx, "eventfd_read", ci, [ci, cv])(efd, ctypes.byref(val)), 0); check("eventfd value", val.value, 5)
    check("eventfd_write", fn(mlx, "eventfd_write", ci, [ci, ctypes.c_uint64])(efd, 7), 0)
    fn(mlx, "eventfd_read", ci, [ci, cv])(efd, ctypes.byref(val)); check("eventfd value 2", val.value, 7)
    cl_(efd)
    un = ctypes.create_string_buffer(390)
    check("uname", fn(mlx, "uname", ci, [cv])(un), 0)
    check("uname nodename", un.raw[65:65 + len(os.uname().nodename)], os.uname().nodename.encode())
    host = ctypes.create_string_buffer(64)
    check("gethostname", fn(mlx, "gethostname", ci, [cv, cs])(host, 64), 0); check("hostname", host.value.decode(), os.uname().nodename)
    sysconf = fn(mlx, "sysconf", cl, [ci])
    check("sysconf pagesize", sysconf(30), 4096)
    check("sysconf cpus", sysconf(84), os.cpu_count())
    check("sysconf open max", sysconf(4) > 0, True)
    check("sysconf unknown", sysconf(9999), -1)
    rnd = ctypes.create_string_buffer(16)
    check("getrandom", fn(mlx, "getrandom", ctypes.c_ssize_t, [cv, cs, ctypes.c_uint])(rnd, 16, 0), 16)
    check("getrandom random", rnd.raw != b"\0" * 16, True)
    check("getentropy", fn(mlx, "getentropy", ci, [cv, cs])(rnd, 16), 0)
    check("getentropy EIO", fn(mlx, "getentropy", ci, [cv, cs])(rnd, 300), -1); check("EIO", el()[0], 5)
    mm = fn(mlx, "mmap", cv, [cv, cs, ci, ci, ci, cl])(None, 8192, 3, 0x22, -1, 0)
    check("mmap", mm is not None and mm != 2**64 - 1, True)
    ctypes.memset(mm, 7, 8192)
    check("munmap", fn(mlx, "munmap", ci, [cv, cs])(mm, 8192), 0)
    el()[0] = 0
    check("mmap fails", fn(mlx, "mmap", ctypes.c_uint64, [cv, cs, ci, ci, ci, cl])(None, 8192, 3, 0x02, 12345, 0), 2**64 - 1); check("EBADF", el()[0], 9)
    pgs = fn(mlx, "getpagesize", ci, [])(); check("getpagesize", pgs, 4096)
    rl = (ctypes.c_uint64 * 2)()
    check("getrlimit", fn(mlx, "getrlimit", ci, [ci, cv])(7, rl), 0)
    import resource
    check("getrlimit value", rl[0], resource.getrlimit(resource.RLIMIT_NOFILE)[0])
    sc = fn(mlx, "syscall", cl, [cl, cs, cs, cs, cs, cs, cs])
    check("syscall getpid", sc(39, 0, 0, 0, 0, 0, 0), os.getpid())
    check("syscall bad", sc(9999, 0, 0, 0, 0, 0, 0), -1); check("ENOSYS", el()[0], 38)

if wanted("signal"):
    import signal as pysignal
    sigset = ctypes.create_string_buffer(128)
    check("sigemptyset", fn(mlx, "sigemptyset", ci, [cv])(sigset), 0)
    check("sigaddset", fn(mlx, "sigaddset", ci, [cv, ci])(sigset, 10), 0)
    check("sigaddset 40", fn(mlx, "sigaddset", ci, [cv, ci])(sigset, 40), 0)
    check("sigismember", fn(mlx, "sigismember", ci, [cv, ci])(sigset, 10), 1)
    check("sigismember 40", fn(mlx, "sigismember", ci, [cv, ci])(sigset, 40), 1)
    check("sigismember no", fn(mlx, "sigismember", ci, [cv, ci])(sigset, 11), 0)
    check("sigismember bad", fn(mlx, "sigismember", ci, [cv, ci])(sigset, 99), -1)
    check("sigset words", (ctypes.cast(sigset, ctypes.POINTER(ctypes.c_uint64))[0]), (1 << 9) | (1 << 39))
    check("sigdelset", fn(mlx, "sigdelset", ci, [cv, ci])(sigset, 10), 0); check("sigdelset gone", fn(mlx, "sigismember", ci, [cv, ci])(sigset, 10), 0)
    seen = []
    HANDLER = ctypes.CFUNCTYPE(None, ci)
    @HANDLER
    def handler(number): seen.append(number)
    old = fn(mlx, "signal", cv, [ci, HANDLER])(pysignal.SIGUSR1, handler)
    fn(mlx, "raise", ci, [ci])(pysignal.SIGUSR1)
    check("signal handler ran", seen, [pysignal.SIGUSR1])
    # sigaction reads back what signal set, and the handler returns through the restorer.
    act = ctypes.create_string_buffer(152)
    check("sigaction read", fn(mlx, "sigaction", ci, [ci, cv, cv])(pysignal.SIGUSR1, None, act), 0)
    check("sigaction handler", ctypes.cast(act, ctypes.POINTER(cv))[0], ctypes.cast(handler, cv).value)
    check("sigaction SA_RESTART", ctypes.cast(ctypes.addressof(act) + 136, ctypes.POINTER(ci))[0] & 0x10000000, 0x10000000)
    # Blocked: the raise is held until unblocked.
    mask = ctypes.create_string_buffer(128); fn(mlx, "sigemptyset", ci, [cv])(mask); fn(mlx, "sigaddset", ci, [cv, ci])(mask, pysignal.SIGUSR1)
    check("sigprocmask block", fn(mlx, "sigprocmask", ci, [ci, cv, cv])(0, mask, None), 0)
    fn(mlx, "raise", ci, [ci])(pysignal.SIGUSR1)
    check("blocked", seen, [pysignal.SIGUSR1])
    pending = ctypes.create_string_buffer(128)
    check("sigpending", fn(mlx, "sigpending", ci, [cv])(pending), 0); check("pending member", fn(mlx, "sigismember", ci, [cv, ci])(pending, pysignal.SIGUSR1), 1)
    check("sigprocmask unblock", fn(mlx, "sigprocmask", ci, [ci, cv, cv])(1, mask, None), 0)
    check("delivered", seen, [pysignal.SIGUSR1, pysignal.SIGUSR1])
    # Back to the default (SIG_DFL = 0) through signal(), which answers the old handler.
    check("signal old", fn(mlx, "signal", cv, [ci, cv])(pysignal.SIGUSR1, 0), ctypes.cast(handler, cv).value)
    check("sigrtmin", fn(mlx, "__libc_current_sigrtmin", ci, [])(), 34)


if wanted("printf"):
    # snprintf through ctypes: the variadic call puts doubles in xmm0..7 and
    # the rest in the integer registers and on the stack, as C does.
    m = fn(mlx, "snprintf", ci, None); g = fn(glibc, "snprintf", ci, None)
    def both_snprintf(fmt, *args):
        b1 = ctypes.create_string_buffer(512); b2 = ctypes.create_string_buffer(512)
        r1 = m(b1, 512, fmt, *args); r2 = g(b2, 512, fmt, *args)
        check("snprintf", (r1, b1.value), (r2, b2.value), f"({fmt!r}, {args!r})")
    ints = [0, 1, -1, 42, -42, 2147483647, -2147483648, 65535, 255]
    for v in ints:
        for fmt in [b"%d", b"%i", b"%5d", b"%-5d|", b"%05d", b"%+d", b"% d", b"%.3d", b"%8.3d", b"%-8.3d|", b"%x", b"%X", b"%#x", b"%#08x", b"%o", b"%#o", b"%u", b"%c", b"%hhd", b"%hd", b"%5.0d", b"%.0d"]:
            both_snprintf(fmt, ci(v))
    for v in [0, 1, -1, 1 << 40, -(1 << 40), 9223372036854775807, -9223372036854775808]:
        for fmt in [b"%ld", b"%lld", b"%lu", b"%lx", b"%20ld", b"%-20ld|", b"%020ld", b"%zu", b"%jd", b"%td", b"%#lx", b"%llX"]:
            both_snprintf(fmt, cl(v))
    for text in [b"", b"hello", b"hello world", b"x"]:
        for fmt in [b"%s", b"%10s", b"%-10s|", b"%.3s", b"%10.2s|", b"%.0s|", b"[%s]"]:
            both_snprintf(fmt, cp(text))
    both_snprintf(b"%s", cp(None))
    both_snprintf(b"%.3s", cp(None))
    both_snprintf(b"%p", cv(0x1234)); both_snprintf(b"%p", cv(None)); both_snprintf(b"%20p|", cv(0xdeadbeef)); both_snprintf(b"%-20p|", cv(0xdeadbeef))
    both_snprintf(b"%%"); both_snprintf(b"100%% done"); both_snprintf(b"plain")
    both_snprintf(b"%*d", ci(6), ci(42)); both_snprintf(b"%-*d|", ci(6), ci(42)); both_snprintf(b"%.*f", ci(2), cd(3.14159)); both_snprintf(b"%*.*f", ci(10), ci(3), cd(2.5)); both_snprintf(b"%*d", ci(-6), ci(42))
    both_snprintf(b"%d %s %c %x %ld %f", ci(1), cp(b"two"), ci(51), ci(255), cl(5), cd(6.5))
    both_snprintf(b"%d %d %d %d %d %d %d %d %d", ci(1), ci(2), ci(3), ci(4), ci(5), ci(6), ci(7), ci(8), ci(9))
    both_snprintf(b"%d %f %d %f %d %f %d", ci(1), cd(1.5), ci(2), cd(2.5), ci(3), cd(3.5), ci(4))
    import math
    floats = [0.0, -0.0, 1.0, -1.0, 0.5, 3.14159, -2.71828, 1e10, 1e-10, 123456789.125, 0.1, 0.3, 2.0/3.0, 1e300, 1e-300, 1.7976931348623157e308, 5e-324, 2.2250738585072014e-308, 999999.5, 0.000012345, 1234567.0, 100.0, 1e21, 1e22, 1e23, 4503599627370496.0, 0.125, 0.0625, 1.5, 2.5, -0.5, 1e-5, 1e-4, 123.456, 9.999999, 99.99, 0.99999999, float("inf"), float("-inf"), float("nan")]
    for v in floats:
        for fmt in [b"%f", b"%.0f", b"%.1f", b"%.2f", b"%.10f", b"%10.3f", b"%-10.3f|", b"%+f", b"% f", b"%010.2f", b"%#.0f", b"%F", b"%e", b"%.0e", b"%.3e", b"%E", b"%15.4e", b"%-15.4e|", b"%+.2e", b"%g", b"%.3g", b"%.10g", b"%G", b"%#g", b"%#.3g", b"%12g", b"%-12g|", b"%.0g", b"%.1g", b"%a", b"%.3a", b"%A", b"%.0a", b"%.20f", b"%.17g", b"%.30f"]:
            if math.isnan(v) and fmt in (b"%a", b"%.3a", b"%A", b"%.0a"): continue
            both_snprintf(fmt, cd(v))
    # The count of a truncated snprintf, and sprintf/asprintf agreeing.
    b1 = ctypes.create_string_buffer(8); b2 = ctypes.create_string_buffer(8)
    check("snprintf truncates", (m(b1, 8, b"%s and %d", cp(b"hello"), ci(12345)), b1.raw), (g(b2, 8, b"%s and %d", cp(b"hello"), ci(12345)), b2.raw))
    check("snprintf zero size", m(None, 0, b"%d", ci(12345)), g(None, 0, b"%d", ci(12345)))
    sp = fn(mlx, "sprintf", ci, None); b1 = ctypes.create_string_buffer(64)
    check("sprintf", (sp(b1, b"%d-%s", ci(7), cp(b"x")), b1.value), (3, b"7-x"))
    asp = fn(mlx, "asprintf", ci, None); out = cp()
    check("asprintf", (asp(ctypes.byref(out), b"%s=%d", cp(b"key"), ci(99)), out.value), (6, b"key=99"))
    fn(mlx, "free", None, [cv])(ctypes.cast(out, cv))
    # vsnprintf through a va_list made by glibc's vasprintf? Simpler: the
    # library's own vsnprintf via a C caller is checked by the C program.
    # dprintf to a pipe.
    r, w = os.pipe()
    dp = fn(mlx, "dprintf", ci, None)
    check("dprintf", dp(w, b"[%5.1f|%s]", cd(2.25), cp(b"ok")), 10)
    os.close(w); check("dprintf wrote", os.read(r, 64), b"[  2.2|ok]"); os.close(r)
    # The %n and %m conversions.
    n = ci(0); b1 = ctypes.create_string_buffer(64)
    m(b1, 64, b"abc%n", ctypes.byref(n)); check("%n", n.value, 3)
    el = fn(mlx, "__errno_location", ctypes.POINTER(ci), []); el()[0] = 2
    m(b1, 64, b"%m"); check("%m", b1.value, b"No such file or directory")
    # sscanf.
    ss = fn(mlx, "sscanf", ci, None); gs = fn(glibc, "sscanf", ci, None)
    for text, fmt, kinds in [(b"12 34", b"%d %d", "ii"), (b"  -7x", b"%d%c", "ic"), (b"0x1f 077 99", b"%i %i %i", "iii"), (b"abc def", b"%s %s", "ss"), (b"3.5 -2e3", b"%f %lf", "fd"), (b"key=value", b"%[^=]=%s", "ss"), (b"12abc", b"%2d%s", "is"), (b"ff", b"%x", "i"), (b"", b"%d", "i"), (b"word", b"%d", "i"), (b"42", b"%*d%n", "i"), (b"a,b", b"%c,%c", "cc"), (b"1234567890123", b"%ld", "l"), (b"  42  ", b"%d", "i"), (b"x=5", b"x=%d", "i"), (b"y=5", b"x=%d", "i")]:
        def run(f):
            holders = []
            for k in kinds:
                holders.append({"i": ci, "c": ctypes.c_char, "s": lambda: ctypes.create_string_buffer(64), "f": ctypes.c_float, "d": cd, "l": cl}[k]())
            r = f(text, fmt, *[ctypes.byref(h) if not isinstance(h, ctypes.Array) else h for h in holders])
            vals = []
            for h in holders:
                vals.append(h.value if not isinstance(h, ctypes.Array) else h.value)
            return (r, vals)
        check("sscanf", run(ss), run(gs), f"({text!r}, {fmt!r})")

if wanted("stdio"):
    work = tempfile.mkdtemp(prefix="mlxlibc-stdio-")
    path = (work + "/out.txt").encode()
    fopen = fn(mlx, "fopen", cv, [cp, cp]); fclose = fn(mlx, "fclose", ci, [cv]); fputs = fn(mlx, "fputs", ci, [cp, cv]); fwrite = fn(mlx, "fwrite", cs, [cv, cs, cs, cv])
    fprintf = fn(mlx, "fprintf", ci, None); fflush = fn(mlx, "fflush", ci, [cv]); fputc = fn(mlx, "fputc", ci, [ci, cv]); ftell = fn(mlx, "ftell", cl, [cv]); fseek = fn(mlx, "fseek", ci, [cv, cl, ci])
    f = fopen(path, b"w")
    check("fopen w", f is not None, True)
    check("fputs", fputs(b"line one\n", f), 1)
    check("fprintf", fprintf(cv(f), b"%s %d %.2f\n", cp(b"line"), ci(2), cd(2.5)), 12)
    check("fwrite", fwrite(b"line three\n", 1, 11, f), 11)
    check("fputc", fputc(0x41, f), 0x41)
    check("ftell", ftell(f), 33)
    check("file not yet flushed", os.path.getsize(path) == 0 or os.path.getsize(path) == 33, True)
    check("fclose", fclose(f), 0)
    check("file content", open(path, "rb").read(), b"line one\nline 2 2.50\nline three\nA")
    f = fopen(path, b"r")
    fgets = fn(mlx, "fgets", cv, [cv, ci, cv]); fgetc = fn(mlx, "fgetc", ci, [cv]); ungetc = fn(mlx, "ungetc", ci, [ci, cv]); fread = fn(mlx, "fread", cs, [cv, cs, cs, cv]); feof = fn(mlx, "feof", ci, [cv])
    buf = ctypes.create_string_buffer(64)
    check("fgets", (fgets(buf, 64, f) is not None, buf.value), (True, b"line one\n"))
    check("fgetc", fgetc(f), ord("l"))
    check("ungetc", ungetc(ord("L"), f), ord("L"))
    check("fgets after ungetc", (fgets(buf, 6, f) is not None, buf.value), (True, b"Line "))
    check("ftell read", ftell(f), 14)
    check("fseek", fseek(f, -5, 1), 0)
    check("fread", (fread(buf, 1, 4, f), buf.raw[:4]), (4, b"line"))
    check("fseek end", fseek(f, 0, 2), 0); check("ftell end", ftell(f), 33)
    check("fgetc eof", fgetc(f), -1); check("feof", feof(f), 1)
    check("fseek set", fseek(f, 0, 0), 0); check("feof cleared", feof(f), 0)
    line = cp(); cap = cs(0)
    getline = fn(mlx, "getline", ctypes.c_ssize_t, [ctypes.POINTER(cp), ctypes.POINTER(cs), cv])
    check("getline", (getline(ctypes.byref(line), ctypes.byref(cap), f), line.value), (9, b"line one\n"))
    check("getline 2", (getline(ctypes.byref(line), ctypes.byref(cap), f), line.value), (12, b"line 2 2.50\n"))
    fn(mlx, "free", None, [cv])(ctypes.cast(line, cv))
    fclose(f)
    check("fopen missing", fopen(b"/no/such/file.txt", b"r"), None)
    check("fopen bad mode", fopen(path, b"q"), None)
    f = fopen(path, b"a"); fputs(b"appended\n", f); fclose(f)
    check("append", open(path, "rb").read().endswith(b"Aappended\n"), True)
    # fscanf on a file, fileno, setvbuf, remove, tmpfile, perror text.
    f = fopen(path, b"r"); fscanf = fn(mlx, "fscanf", ci, None); w1 = ctypes.create_string_buffer(16); w2 = ctypes.create_string_buffer(16)
    check("fscanf", (fscanf(cv(f), b"%s %s", w1, w2), w1.value, w2.value), (2, b"line", b"one"))
    check("fileno", fn(mlx, "fileno", ci, [cv])(f) > 2, True)
    fclose(f)
    f = fopen(path, b"w"); check("setvbuf", fn(mlx, "setvbuf", ci, [cv, cv, ci, cs])(f, None, 2, 0), 0); fputs(b"unbuffered", f); check("unbuffered written", open(path, "rb").read(), b"unbuffered"); fclose(f)
    t = fn(mlx, "tmpfile", cv, [])(); check("tmpfile", t is not None, True); fputs(b"tmp", t); check("tmpfile rewind", fseek(t, 0, 0), 0); check("tmpfile read", (fread(buf, 1, 3, t), buf.raw[:3]), (3, b"tmp")); fclose(t)
    check("remove", fn(mlx, "remove", ci, [cp])(path), 0); check("removed", os.path.exists(path), False)
    check("remove dir", fn(mlx, "remove", ci, [cp])(work.encode()), 0)
    # The standard streams: the data symbols hold their records' addresses
    # (set by the library's DT_INIT when the dynamic linker loaded it), and
    # the records have glibc's _IO_FILE layout where C's inline putc and
    # getc look (_flags at 0, _fileno at 112).
    # (Loaded next to glibc, the library leaves `stdout` and the others to
    # glibc's values; its own records are asked for by descriptor.)
    stream = fn(mlx, "mlxlibc_stream", cv, [ci]); out = stream(1); err = stream(2); inp = stream(0)
    check("stdout set", out is not None and err is not None and inp is not None and len({out, err, inp}) == 3, True)
    check("stdout _fileno", ctypes.c_int.from_address(out + 112).value, 1)
    check("stderr _fileno", ctypes.c_int.from_address(err + 112).value, 2)
    check("stdout _flags magic", ctypes.c_uint.from_address(out).value & 0xFFFF0000, 4222222336)
    r, w = os.pipe(); saved = os.dup(1); os.dup2(w, 1)
    try:
        pf = fn(mlx, "printf", ci, None); puts = fn(mlx, "puts", ci, [cp]); putchar = fn(mlx, "putchar", ci, [ci])
        pf(b"via printf %d\n", ci(1)); puts(b"via puts"); putchar(ord("Z")); fn(mlx, "fflush", ci, [cv])(ctypes.c_void_p(out))
    finally:
        os.dup2(saved, 1); os.close(saved); os.close(w)
    check("stdout output", os.read(r, 256), b"via printf 1\nvia puts\nZ"); os.close(r)
    # perror writes to stderr unbuffered.
    r, w = os.pipe(); saved = os.dup(2); os.dup2(w, 2)
    try:
        el = fn(mlx, "__errno_location", ctypes.POINTER(ci), []); el()[0] = 13
        fn(mlx, "perror", None, [cp])(b"open")
    finally:
        os.dup2(saved, 2); os.close(saved); os.close(w)
    check("perror", os.read(r, 256), b"open: Permission denied\n"); os.close(r)

if wanted("time"):
    tm1 = ctypes.create_string_buffer(56); tm2 = ctypes.create_string_buffer(56)
    now = cl(int(time.time()))
    check("time", abs(fn(mlx, "time", cl, [cv])(None) - int(time.time())) <= 1, True)
    for stamp in [0, 1, 86399, 86400, 951782400, 1234567890, 1700000000, 2147483647, 4102444800, -1, -86401, 253402300799]:
        t = cl(stamp)
        fn(mlx, "gmtime_r", cv, [cv, cv])(ctypes.byref(t), tm1); fn(glibc, "gmtime_r", cv, [cv, cv])(ctypes.byref(t), tm2)
        check("gmtime_r", tm1.raw[:36] + tm1.raw[40:48], tm2.raw[:36] + tm2.raw[40:48], f"({stamp})")  # bytes 36..40 are padding
        check("gmtime_r zone", ctypes.string_at(ctypes.cast(ctypes.addressof(tm1) + 48, ctypes.POINTER(cv))[0]), ctypes.string_at(ctypes.cast(ctypes.addressof(tm2) + 48, ctypes.POINTER(cv))[0]), f"({stamp})")
        fn(mlx, "localtime_r", cv, [cv, cv])(ctypes.byref(t), tm1); fn(glibc, "localtime_r", cv, [cv, cv])(ctypes.byref(t), tm2)
        check("localtime_r", tm1.raw[:36], tm2.raw[:36], f"({stamp})")
        check("timegm", fn(mlx, "timegm", cl, [cv])(tm1), fn(glibc, "timegm", cl, [cv])(tm2), f"({stamp})")
        fn(mlx, "localtime_r", cv, [cv, cv])(ctypes.byref(t), tm1); fn(glibc, "localtime_r", cv, [cv, cv])(ctypes.byref(t), tm2)
        check("mktime", fn(mlx, "mktime", cl, [cv])(tm1), fn(glibc, "mktime", cl, [cv])(tm2), f"({stamp})")
        fn(mlx, "gmtime_r", cv, [cv, cv])(ctypes.byref(t), tm1); fn(glibc, "gmtime_r", cv, [cv, cv])(ctypes.byref(t), tm2)
        for fmt in [b"%Y-%m-%d %H:%M:%S", b"%a %A %b %B %h", b"%c", b"%D %F %T %R %r", b"%d %e %j %m %y %C %u %w", b"%I %l %k %p %P", b"%U %W %V %G %g", b"%s", b"%z %Z", b"%n%t%%", b"%x %X", b"%M:%S"]:
            b1 = ctypes.create_string_buffer(128); b2 = ctypes.create_string_buffer(128)
            r1 = fn(mlx, "strftime", cs, [cv, cs, cp, cv])(b1, 128, fmt, tm1); r2 = fn(glibc, "strftime", cs, [cv, cs, cp, cv])(b2, 128, fmt, tm2)
            check("strftime", (r1, b1.value), (r2, b2.value), f"({stamp}, {fmt!r})")
        a1 = ctypes.create_string_buffer(32); a2 = ctypes.create_string_buffer(32)
        fn(mlx, "asctime_r", cv, [cv, cv])(tm1, a1); fn(glibc, "asctime_r", cv, [cv, cv])(tm2, a2)
        check("asctime_r", a1.value, a2.value, f"({stamp})")
    b1 = ctypes.create_string_buffer(4)
    check("strftime too small", fn(mlx, "strftime", cs, [cv, cs, cp, cv])(b1, 4, b"%Y-%m-%d", tm1), 0)
    tv = (cl * 2)()
    check("gettimeofday", fn(mlx, "gettimeofday", ci, [cv, cv])(tv, None), 0); check("gettimeofday near", abs(tv[0] - int(time.time())) <= 1, True)
    ts = (cl * 2)()
    check("clock_gettime", fn(mlx, "clock_gettime", ci, [ci, cv])(1, ts), 0); check("monotonic", ts[0] > 0, True)
    check("clock_gettime bad", fn(mlx, "clock_gettime", ci, [ci, cv])(99, ts), -1)
    started = time.monotonic()
    req = (cl * 2)(0, 20_000_000)
    check("nanosleep", fn(mlx, "nanosleep", ci, [cv, cv])(req, None), 0)
    check("nanosleep slept", time.monotonic() - started >= 0.019, True)
    check("usleep", fn(mlx, "usleep", ci, [ctypes.c_ulong])(1000), 0)
    check("clock", fn(mlx, "clock", cl, [])() > 0, True)
    check("difftime", fn(mlx, "difftime", cd, [cl, cl])(10, 3), 7.0)

if wanted("pthread"):
    # Threads mlxlibc starts have mlxlibc's thread block behind the thread
    # pointer, not glibc's, so Python (whose callbacks need glibc's TLS) must
    # not run in them: their bodies are the library's own exports (strlen,
    # pthread_barrier_wait, pthread_getspecific), while Python's own threads
    # (glibc threads, foreign to mlxlibc) exercise the mutexes and condition
    # variables from outside.
    counter = [0]
    mutex = ctypes.create_string_buffer(40)
    pml = fn(mlx, "pthread_mutex_lock", ci, [cv]); pmu = fn(mlx, "pthread_mutex_unlock", ci, [cv])
    def worker():
        for _ in range(2000):
            pml(mutex); counter[0] += 1; pmu(mutex)
    workers = [threading.Thread(target=worker) for _ in range(4)]
    for w in workers: w.start()
    for w in workers: w.join()
    check("mutex counted", counter[0], 8000)
    pc = fn(mlx, "pthread_create", ci, [ctypes.POINTER(cv), cv, cv, cv]); pj = fn(mlx, "pthread_join", ci, [cv, ctypes.POINTER(cv)])
    strlen_body = ctypes.cast(mlx.strlen, cv)
    words = [b"", b"hello", b"hello world", b"a" * 300]
    threads = []
    for word in words:
        t = cv()
        check("pthread_create", pc(ctypes.byref(t), None, strlen_body, ctypes.cast(cp(word), cv)), 0)
        threads.append(t)
    for word, t in zip(words, threads):
        result = cv()
        check("pthread_join", pj(t, ctypes.byref(result)), 0)
        check("thread result", result.value or 0, len(word), f"({word[:8]!r})")
    check("join twice ESRCH", pj(threads[1], None) in (3, 22), True)
    check("pthread_self", fn(mlx, "pthread_self", cv, [])() == fn(glibc, "pthread_self", cv, [])(), True)
    check("pthread_equal", fn(mlx, "pthread_equal", ci, [cv, cv])(5, 5), 1)
    # A detached thread, then its stack reused by the next one.
    pd = fn(mlx, "pthread_detach", ci, [cv]); t = cv()
    check("create detached", pc(ctypes.byref(t), None, strlen_body, ctypes.cast(cp(b"detached"), cv)), 0); check("detach", pd(t), 0)
    time.sleep(0.2)
    check("join detached EINVAL", pj(t, None) in (3, 22), True)
    t = cv(); check("create after detached", pc(ctypes.byref(t), None, strlen_body, ctypes.cast(cp(b"xyz"), cv)), 0); result = cv(); pj(t, ctypes.byref(result)); check("result after detached", result.value, 3)
    # Recursive and error-checking mutexes.
    attr = ctypes.create_string_buffer(8); rm = ctypes.create_string_buffer(40)
    fn(mlx, "pthread_mutexattr_init", ci, [cv])(attr); check("settype recursive", fn(mlx, "pthread_mutexattr_settype", ci, [cv, ci])(attr, 1), 0)
    check("mutex_init", fn(mlx, "pthread_mutex_init", ci, [cv, cv])(rm, attr), 0)
    check("recursive lock", (pml(rm), pml(rm), pmu(rm), pmu(rm)), (0, 0, 0, 0))
    check("trylock free", fn(mlx, "pthread_mutex_trylock", ci, [cv])(mutex), 0); check("trylock busy", fn(mlx, "pthread_mutex_trylock", ci, [cv])(mutex) == 16, True); pmu(mutex)
    fn(mlx, "pthread_mutexattr_settype", ci, [cv, ci])(attr, 2); em = ctypes.create_string_buffer(40); fn(mlx, "pthread_mutex_init", ci, [cv, cv])(em, attr)
    check("errorcheck deadlock", (pml(em), pml(em)), (0, 35)); pmu(em); check("errorcheck unlock twice EPERM", pmu(em), 1)
    # once (the init routine runs in the calling thread, a glibc one here).
    once = ci(0); ran = ci(0)
    ONCE = ctypes.CFUNCTYPE(None)
    @ONCE
    def init_once(): ran.value += 1
    po = fn(mlx, "pthread_once", ci, [cv, ONCE])
    check("once", (po(ctypes.byref(once), init_once), po(ctypes.byref(once), init_once), ran.value), (0, 0, 1))
    # condition variable: a Python thread waits for a flag.
    cond = ctypes.create_string_buffer(48); cmutex = ctypes.create_string_buffer(40); flag = [0]; seen = [0]
    pcw = fn(mlx, "pthread_cond_wait", ci, [cv, cv]); pcs = fn(mlx, "pthread_cond_signal", ci, [cv])
    def waiter():
        pml(cmutex)
        while flag[0] == 0: pcw(cond, cmutex)
        seen[0] = flag[0]
        pmu(cmutex)
    w = threading.Thread(target=waiter); w.start()
    time.sleep(0.05)
    pml(cmutex); flag[0] = 9; pcs(cond); pmu(cmutex)
    w.join(5); check("cond seen", seen[0], 9)
    deadline = (cl * 2)(0, 0); fn(mlx, "clock_gettime", ci, [ci, cv])(0, deadline); deadline[1] += 20_000_000
    if deadline[1] >= 1_000_000_000: deadline[0] += 1; deadline[1] -= 1_000_000_000
    pml(cmutex); check("cond timedwait", fn(mlx, "pthread_cond_timedwait", ci, [cv, cv, cv])(cond, cmutex, deadline), 110); pmu(cmutex)
    # keys: set in this thread, unset in a new one (whose body is getspecific).
    key = ctypes.c_uint(0)
    check("key_create", fn(mlx, "pthread_key_create", ci, [cv, cv])(ctypes.byref(key), None), 0)
    check("setspecific", fn(mlx, "pthread_setspecific", ci, [ctypes.c_uint, cv])(key, 0x55), 0)
    check("getspecific", fn(mlx, "pthread_getspecific", cv, [ctypes.c_uint])(key), 0x55)
    t = cv(); pc(ctypes.byref(t), None, ctypes.cast(mlx.pthread_getspecific, cv), key.value); result = cv(0x77); pj(t, ctypes.byref(result))
    check("key per thread", result.value, None)
    # rwlock, spin, barrier, semaphore
    rw = ctypes.create_string_buffer(56)
    check("rdlock", (fn(mlx, "pthread_rwlock_rdlock", ci, [cv])(rw), fn(mlx, "pthread_rwlock_rdlock", ci, [cv])(rw), fn(mlx, "pthread_rwlock_trywrlock", ci, [cv])(rw)), (0, 0, 16))
    check("rwunlock", (fn(mlx, "pthread_rwlock_unlock", ci, [cv])(rw), fn(mlx, "pthread_rwlock_unlock", ci, [cv])(rw), fn(mlx, "pthread_rwlock_trywrlock", ci, [cv])(rw), fn(mlx, "pthread_rwlock_tryrdlock", ci, [cv])(rw), fn(mlx, "pthread_rwlock_unlock", ci, [cv])(rw)), (0, 0, 0, 16, 0))
    spin = ci(0)
    check("spin", (fn(mlx, "pthread_spin_lock", ci, [cv])(ctypes.byref(spin)), fn(mlx, "pthread_spin_trylock", ci, [cv])(ctypes.byref(spin)), fn(mlx, "pthread_spin_unlock", ci, [cv])(ctypes.byref(spin))), (0, 16, 0))
    sem = ctypes.create_string_buffer(32)
    check("sem", (fn(mlx, "sem_init", ci, [cv, ci, ctypes.c_uint])(sem, 0, 1), fn(mlx, "sem_wait", ci, [cv])(sem), fn(mlx, "sem_trywait", ci, [cv])(sem), fn(mlx, "sem_post", ci, [cv])(sem), fn(mlx, "sem_wait", ci, [cv])(sem)), (0, 0, -1, 0, 0))
    # Two threads wait at a barrier of three (their body is the wait itself).
    barrier = ctypes.create_string_buffer(32)
    fn(mlx, "pthread_barrier_init", ci, [cv, cv, ctypes.c_uint])(barrier, None, 3)
    pbw = fn(mlx, "pthread_barrier_wait", ci, [cv]); wait_body = ctypes.cast(mlx.pthread_barrier_wait, cv)
    ts = []
    for i in range(2):
        t = cv(); pc(ctypes.byref(t), None, wait_body, ctypes.cast(barrier, cv)); ts.append(t)
    time.sleep(0.05)
    check("barrier holds", fn(mlx, "pthread_tryjoin_np", ci, [cv, cv])(ts[0], None), 16)
    serial = pbw(barrier)
    results = []
    for t in ts:
        r = cv(); pj(t, ctypes.byref(r)); results.append((r.value or 0) & 0xffffffff)
    check("barrier serial once", sorted(results + [serial & 0xffffffff]), [0, 0, 0xffffffff])
    # names
    name = ctypes.create_string_buffer(16)
    check("setname", fn(mlx, "pthread_setname_np", ci, [cv, cp])(fn(mlx, "pthread_self", cv, [])(), b"mlxcheck"), 0)
    check("getname", (fn(mlx, "pthread_getname_np", ci, [cv, cv, cs])(fn(mlx, "pthread_self", cv, [])(), name, 16), name.value), (0, b"mlxcheck"))

print(f"{checked} comparisons, {len(failures)} failures")
for line in failures[:40]: print("  " + line)
sys.exit(1 if failures else 0)
