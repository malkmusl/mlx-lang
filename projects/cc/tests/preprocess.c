/* The examples of C11 6.10.3.5 and more directives, for -E. */
#define x 3
#define f(a) f(x * (a))
#undef x
#define x 2
#define g f
#define z z[0]
#define h g(~ // (renamed below)
#define m(a) a(w)
#define w 0,1
#define t(a) a
#define p() int
#define q(x) x
#define r(x,y) x ## y
#define str(x) # x
f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);
g(x+(3,4)-w) | h 5) & m
(f)^m(m);
p() i[q()] = { q(1), r(2,3), r(4,), r(,5), r(,) };
char c[2][6] = { str(hello), str() };
#define xstr(s) str(s)
#define debug(s, t) printf("x" # s "= %d, x" # t "= %s", \
 x ## s, x ## t)
#define INCFILE(n) vers ## n
#define glue(a, b) a ## b
#define xglue(a, b) glue(a, b)
#define HIGHLOW "hello"
#define LOW LOW ", world"
debug(1, 2);
fputs(str(strncmp("abc\0d", "abc", '\4') // this goes away
 == 0) str(: @\n), s);
xstr(INCFILE(2).hh)
glue(HIGH, LOW);
xglue(HIGH, LOW)
#define showlist(...) puts(#__VA_ARGS__)
#define report(test, ...) ((test)?puts(#test): printf(__VA_ARGS__))
showlist(The first, second, and third items.);
report(x>y, "x is %d but y is %d", x, y);
#if defined(x) && x == 2 && !defined(nothing)
yes_if
#elif 1
no_elif
#else
no_else
#endif
#ifndef x
no
#else
yes_else
#endif
#if (1 ? 2 : 3) == 2 && (5 / 2) == 2 && (-1 < 0) && 'a' == 97 && (1 << 4) == 16
yes_math
#endif
__LINE__ __FILE__
