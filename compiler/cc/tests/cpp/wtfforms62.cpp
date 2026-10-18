// A member function template called with its arguments given (append<g>(v)) from a lambda or a generic lambda in a member template, beside a plain function of the name: the template, on the captured this (JSC's JSON FastStringifier).
#include <stdio.h>

namespace p_glam {
enum class Gap { No, Yes };
template <typename F> void each(int n, const F& f) { for (int i = 0; i < n; i++) if (!f(i)) return; }
template <typename C, int M> struct S {
    C buf[64]; unsigned len = 0; int depth = 0;
    void append(char a, char b, char c, char d) { buf[len++] = a; buf[len++] = b; buf[len++] = c; buf[len++] = d; }
    template <Gap g> void append(int v) {
        if (v > 9) {
            buf[len++] = '{';
            each(2, [&](const auto& i) -> bool {
                if (i == 1) buf[len++] = ',';
                append<g>(v / 10 + i);
                return true;
            });
            buf[len++] = '}';
            return;
        }
        buf[len++] = (C)('0' + v);
        if constexpr (g == Gap::Yes) buf[len++] = ' ';
    }
};
int run() {
    S<char, 1> s; s.append<Gap::No>(47); s.buf[s.len] = 0; printf("%s\n", s.buf);
    S<char, 2> t; t.append<Gap::Yes>(23); t.buf[t.len] = 0; printf("%s\n", t.buf);
    return 0;
}
}

namespace p_glam2 {
enum class Gap { No, Yes };
template <typename F> void each(int n, const F& f) { for (int i = 0; i < n; i++) if (!f(i)) return; }
template <typename C, int M> struct S {
    C buf[64]; unsigned len = 0;
    void append(char a, char b, char c, char d) { buf[len++] = a; buf[len++] = b; buf[len++] = c; buf[len++] = d; }
    template <Gap g> void append(int v) {
        if (v > 9) { append<g>(v / 10); append<g>(v % 10); return; }
        buf[len++] = (C)('0' + v);
    }
    template <Gap g> void viaLambda(int v) { each(1, [&](int i) -> bool { append<g>(v + i); return true; }); }
    template <Gap g> void viaGeneric(int v) { each(1, [&](const auto& i) -> bool { append<g>(v + i); return true; }); }
};
int run() {
    S<char, 1> s; s.append<Gap::No>(47); s.viaLambda<Gap::No>(5); s.viaGeneric<Gap::No>(6); s.buf[s.len] = 0; printf("%s\n", s.buf);
    return 0;
}
}

int main() {
    p_glam::run();
    p_glam2::run();
    return 0;
}
