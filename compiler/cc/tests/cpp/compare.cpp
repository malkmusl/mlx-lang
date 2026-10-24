// C++20's three-way comparison and its neighbours: <=> between numbers,
// mixed signedness, floating point (unordered NaN) and pointers; a
// class's operator<=> and the relational operators rewritten from it;
// if and switch with an init statement; aggregates made from braces
// (T{a, b}, braced locals and globals, empty braces as zero); long long
// and signed char as types of their own in overloads and templates; a
// class template named in a typedef or alias without being instantiated.
#include <stdio.h>
#include <compare>
#include <map>
#include <string>
#include <type_traits>
struct Version {
    int major, minor;
    std::strong_ordering operator<=>(const Version &o) const {
        if (auto c = major <=> o.major; c != 0) return c;
        return minor <=> o.minor;
    }
    bool operator==(const Version &o) const { return major == o.major && minor == o.minor; }
};
const char *name(std::strong_ordering o) { return o < 0 ? "less" : o > 0 ? "greater" : "equal"; }
struct G { int a, b; };
G global{3, 4};
int scalar{5};
int f(long) { return 1; }
int f(long long) { return 2; }
int f(unsigned long long) { return 3; }
int f(int) { return 4; }
int g(char) { return 1; }
int g(signed char) { return 2; }
int g(unsigned char) { return 3; }
template <class T> struct Kind { static const int v = 0; };
template <> struct Kind<long> { static const int v = 1; };
template <> struct Kind<long long> { static const int v = 2; };
template <class T> struct Bad { typename T::type x; };
using NeverMade = Bad<int>;
typedef Bad<double> AlsoNeverMade;
template <class T> struct Good { static const int v = sizeof(T); int m; };
typedef Good<int> GoodInt;
int main() {
    int a = 3, b = 5;
    printf("%s %s %s\n", name(a <=> b), name(b <=> a), name(a <=> a));
    unsigned u = 1; long l = -1;
    printf("%s\n", name(l <=> (long)u));
    double x = 1.5, y = 0.0 / 0.0;
    auto p = x <=> 2.5;
    auto q = x <=> y;
    printf("%d %d %d\n", (int)(p < 0), (int)(q == 0), (int)(q < 0 || q > 0 || q == 0));
    Version v1{1, 2}, v2{1, 10}, v3{2, 0};
    printf("%d %d %d %d\n", (int)(v1 < v2), (int)(v3 > v2), (int)(v1 <= v1), (int)(v2 >= v3));
    printf("%s %d\n", name(v1 <=> v3), (int)(v1 == Version{1, 2}));
    int arr[2];
    printf("%s\n", name(&arr[0] <=> &arr[1]));
    if (int k = a * 2; k > 5) printf("init %d\n", k); else printf("no\n");
    switch (int s = b - a; s) { case 2: printf("two\n"); break; default: printf("other\n"); }
    G local{7, 8};
    long zero{};
    int seven{7};
    printf("%d %d %d %d %ld %d\n", global.a + global.b, scalar, local.a, local.b, zero, seven);
    long la = 1; long long lb = 2; unsigned long long lc = 3;
    printf("%d %d %d %d %d\n", f(la), f(lb), f(lc), f(5LL), f(7L));
    signed char sc = 1; unsigned char uc = 2; char c = 3;
    printf("%d %d %d\n", g(c), g(sc), g(uc));
    printf("%d %d %d\n", Kind<long>::v, Kind<long long>::v, Kind<int>::v);
    printf("%d %d\n", (int)std::is_same<long, long long>::value, (int)std::is_same<signed char, char>::value);
    GoodInt gi; gi.m = 4;
    printf("%d %d %d\n", GoodInt::v, gi.m, (int)sizeof(NeverMade *));
    std::map<std::string, int> m; m["b"] = 2; m["a"] = 1; m["c"] = 3;
    for (std::map<std::string, int>::iterator it = m.begin(); it != m.end(); ++it) printf("%s=%d ", it->first.c_str(), it->second);
    printf("\n");
    return 0;
}
