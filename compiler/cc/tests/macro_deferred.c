/* Deferred expansion, as GCC and Clang do it: a macro's name made in one
   expansion and called in the next (the __VA_OPT__ FOR_EACH idiom), and
   the standard's examples next to it. */
#include <stdio.h>
#define PARENS ()
#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__
#define HELPER(macro, a1, ...) macro(a1) __VA_OPT__(, AGAIN PARENS (macro, __VA_ARGS__))
#define AGAIN() HELPER
#define FOR_EACH(macro, ...) __VA_OPT__(EXPAND(HELPER(macro, __VA_ARGS__)))
#define SQ(x) ((x) * (x))
#define STR(x) #x
#define XSTR(x) STR(x)
#define VSTR(...) #__VA_ARGS__
#define XVSTR(...) VSTR(__VA_ARGS__)

#define f(a) a*g
#define g(a) f(a)
#define AA(x) BB(x)
#define BB(x) AA(x) x

int main(void) {
    int squares[] = { FOR_EACH(SQ, 1, 2, 3, 4, 5) };
    int sum = 0;
    for (unsigned i = 0; i < sizeof squares / sizeof squares[0]; i++) sum += squares[i];
    printf("%u %d\n", (unsigned)(sizeof squares / sizeof squares[0]), sum);
    printf("%s\n", XSTR(f(2)(9)));
    printf("%s\n", XSTR(AA(1)));
    printf("%s\n", XVSTR(FOR_EACH(STR, a, b)));
    return 0;
}
