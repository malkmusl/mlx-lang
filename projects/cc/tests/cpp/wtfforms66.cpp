// c ? a : b of two classes, one made from the other (std::nullopt beside std::optional<int>(4)): the result made from either branch, passed by value or used to initialize.
#include <optional>
#include <stdio.h>

namespace p_optc {
static int sub(int start, std::optional<int> end) { return end ? *end * 100 + start : -start; }
static volatile int argcv = 1;
int run() {
    int argc = argcv;
    bool u = argc > 5;
    printf("%d\n", sub(2, u ? std::nullopt : std::optional<int>(4)));
    printf("%d\n", sub(3, std::optional<int>(7)));
    printf("%d\n", sub(4, std::nullopt));
    std::optional<int> o(9); printf("%d\n", sub(5, o));
    printf("%d\n", sub(6, !u ? std::nullopt : std::optional<int>(4)));
    return 0;
}
}

namespace p_optc2 {
struct O { int v; bool has; O() : v(0), has(false) {} O(int x) : v(x), has(true) {} O(const O& o) : v(o.v), has(o.has) {} };
struct N {};
struct P { int v; bool has; P(N) : v(0), has(false) {} P(int x) : v(x), has(true) {} P(const P& o) : v(o.v), has(o.has) {} };
static int subO(O e) { return e.has ? e.v : -1; }
static int subP(P e) { return e.has ? e.v : -1; }
static volatile int argcv = 1;
int run() {
    int argc = argcv;
    bool u = argc > 5;
    printf("%d\n", subO(u ? O() : O(4)));
    printf("%d\n", subP(u ? P(N()) : P(4)));
    printf("%d\n", subP(u ? N() : P(4)));
    P x = u ? N() : P(4); printf("%d\n", x.v);
    return 0;
}
}

int main() {
    p_optc::run();
    p_optc2::run();
    return 0;
}
