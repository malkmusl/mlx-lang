// WTF/JSC forms 27: braced lists choosing constructors, span from ranges, ranges::sort projections, lambdas in generic lambdas, const members of const objects
#include <stdio.h>
#include <span>
#include <stddef.h>
#include <vector>
#include <algorithm>
#include <tuple>

namespace p_bracedspan {
typedef unsigned char Latin1Character;
static int make(std::span<const char16_t> c) { return 16; }
static int make(std::span<const Latin1Character> c) { return 1; }
static int make(std::span<const char> c) { return (int)c.size(); }
int run() { const char* s = "hello"; size_t n = 5; const char16_t* w = u"xy"; printf("%d %d\n", make({ s, n }), make({ w, 2 })); return 0; }
}

namespace p_svvec {
typedef unsigned char LChar;
struct SV {
    SV(std::span<const LChar> s) : n((int)s.size()), wide(false) { }
    SV(std::span<const char16_t> s) : n((int)s.size()), wide(true) { }
    int n; bool wide;
};
int run() { std::vector<LChar> v { 'a', 'b' }; std::vector<char16_t> w { u'x' }; SV a(v); SV b(w); printf("%d %d %d %d\n", a.n, a.wide, b.n, b.wide); return 0; }
}

namespace p_rsortproj {
struct Node { int cell; int other; };
static bool lessThan(int a, int b) { return a < b; }
int run() {
    std::vector<Node> nodes { { 3, 0 }, { 1, 1 }, { 2, 2 } };
    std::ranges::sort(nodes, { }, &Node::cell);
    std::vector<std::tuple<int, int>> b { { 0, 9 }, { 1, 4 }, { 2, 7 } };
    std::ranges::sort(b, lessThan, [](const auto& e) { return std::get<1>(e); });
    printf("%d%d%d %d%d%d\n", nodes[0].cell, nodes[1].cell, nodes[2].cell, std::get<0>(b[0]), std::get<0>(b[1]), std::get<0>(b[2]));
    return 0;
}
}

namespace p_genlam2 {
struct T {
    unsigned m_size { 4 }; int data[4] { 1, 2, 3, 4 };
    template<typename F> void withVector(const F& f) { f(data); }
    template<typename F> static void withVector(int* v, const F& f) { f(v); }
    int rehash() {
        unsigned oldSize = m_size;
        int old[4] = { 10, 20, 30, 40 };
        int sum = 0;
        withVector([&](auto* vector) {
            withVector(old, [&](const auto* oldVector) {
                for (unsigned i = 0; i < oldSize; i++) sum += oldVector[i] + vector[i];
            });
        });
        return sum;
    }
};
int run() { T t; printf("%d\n", t.rehash()); return 0; }
}

namespace p_genlam3 {
template<int N> int f() { auto l = [&] { return N; }; return l(); }
struct S {
    template<int mode> int go() {
        int base = 5;
        auto sweep = [&]<bool destroy>() __attribute__((__always_inline__)) {
            int t = 0;
            auto push = [&](int x) { if constexpr (destroy) t += x * 2; else t += x; };
            push(base);
            return t;
        };
        return sweep.template operator()<true>() + sweep.template operator()<false>();
    }
};
int run() { S s; printf("%d %d\n", f<7>(), s.go<1>()); return 0; }
}

namespace p_genlamnest {
struct Bits { template<typename F> void forEachSetBit(const F& f) const { f(1u); f(4u); } };
struct H {
    unsigned m_atoms { 3 }; unsigned m_start { 1 };
    template<int mode> int sweep(const Bits& live) {
        int total = 0;
        auto sweepBlock = [&]<bool destroy>() {
            unsigned free = m_start;
            auto handle = [&](unsigned index) { if (free != index) total += (int)(index - free); free = index + m_atoms; };
            live.forEachSetBit([&](unsigned index) { handle(index); });
            return destroy ? 100 : 0;
        };
        return sweepBlock.template operator()<true>() + total + mode;
    }
};
int run() { H h; Bits b; printf("%d\n", h.sweep<10>(b)); return 0; }
}

namespace p_spanded3 {
template<typename W, size_t E, typename F> void each(std::span<const W, E> s, const F& f) { for (size_t i = 0; i < s.size(); i++) f(s[i]); }
struct BS { unsigned long bits[2] { 1, 2 }; template<typename F> void forEach(const F& f) const { auto sp = std::span { bits }; each(sp, f); } };
int run() {
    const unsigned long a[2] = { 1, 2 }; unsigned long t = 0;
    auto f = [&](unsigned long w) { t += w; };
    each(std::span { a }, f);
    BS b; b.forEach(f);
    printf("%lu\n", t); return 0;
}
}

int main() {
    p_bracedspan::run();
    p_svvec::run();
    p_rsortproj::run();
    p_genlam2::run();
    p_genlam3::run();
    p_genlamnest::run();
    p_spanded3::run();
    return 0;
}
