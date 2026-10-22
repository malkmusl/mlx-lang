// A constexpr function returning a named local of a class that is not
// trivially copyable: the local is made in the caller's buffer (the
// named return value), whatever the class's size, and the constant
// evaluator reads it there.
#include <cstdio>

struct Small {
    constexpr Small(const char* s, unsigned long k) : p(s), n(k) { }
    constexpr Small(const Small& o) : p(o.p), n(o.n) { }
    static constexpr Small from(const char* s) { return Small { s, 3 }; }
    constexpr unsigned long length() const { return n; }
    const char* p;
    unsigned long n;
};

struct Large {
    constexpr Large(const char* s, unsigned long k) : p(s), n(k), pad(7) { }
    constexpr Large(const Large& o) : p(o.p), n(o.n), pad(o.pad) { }
    static constexpr Large from(const char* s) { return Large { s, 5 }; }
    const char* p;
    unsigned long n;
    long pad;
};

constexpr Small small(const char* c) { auto r = Small::from(c); r.n += 1; return r; }
constexpr Large large(const char* c) { auto r = Large::from(c); return r; }

int main()
{
    constexpr auto s = small("abc");
    static_assert(s.length() == 4);
    constexpr auto l = large("hello");
    static_assert(l.n == 5 && l.pad == 7);
    std::printf("%s %lu %s %lu %ld\n", s.p, s.length(), l.p, l.n, l.pad);
    Small t = small("run");
    std::printf("%s %lu\n", t.p, t.n);
    return 0;
}
