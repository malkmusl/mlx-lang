// Explicit template arguments whose kind differs from the first template
// of the name: `&S::a` and a function template's instance are values,
// read as such though the first template takes types (WebKit's
// SVGPropertyOwnerRegistry::registerProperty, VTTScanner's skipWhile).
#include <cstdio>
struct S { int a; int b; long c; };
template<typename T, typename U> int pick(T, U) { return 1; }
template<int S::*p> int pick() { return 2; }
template<int S::*p, int S::*q> int pick() { return 3; }
template<int S::*p, typename E, long S::*q> int pick() { return 4; }

template<bool pred(char)> bool adapt(char c) { return pred(c); }
bool isA(char c) { return c == 'a'; }
template<typename C, typename D> int skip(C&, D) { return 10; }
template<bool pred(char), typename C> int skip(C& c) { return pred(c[0]) ? 11 : 12; }

template<bool pred(char)> int collect() {
    const char* s = "abc";
    return skip<adapt<pred>>(s);
}

int main() {
    std::printf("%d %d %d\n", pick<&S::a>(), pick<&S::a, &S::b>(), pick<&S::a, int, &S::c>());
    const char* s = "abc";
    std::printf("%d %d %d %d\n", pick<int, int>(1, 2), skip<isA>(s), skip<adapt<isA>>(s), collect<isA>());
    std::printf("%d\n", skip(s, 1));
    return 0;
}
