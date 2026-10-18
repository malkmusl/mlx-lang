// Structured bindings (C++17): of an aggregate, a std::pair, a
// std::tuple, a std::array and a built-in array; by value, by reference
// and const; in range-for over a map and a vector. `return {a, b};`
// for an aggregate, a class with a constructor and a pair; nested
// braces in an initializer list. Weak and inline variables.
#include <stdio.h>
#include <tuple>
#include <utility>
#include <map>
#include <vector>
#include <string>
#include <array>

struct YMD { int y, m, d; };
static YMD ymd(int days) { return { 2000 + days / 365, (days / 30) % 12 + 1, days % 30 + 1 }; }
static std::tuple<int, double, const char *> three() { return std::make_tuple(7, 2.5, "seven"); }
struct Pt { int x; int y; };
struct C { C(int a, int b) : s(a + b) {} int s; };
static C made(int x) { return { x, 1 }; }
static std::pair<int, int> twoInts() { return { 4, 5 }; }

__attribute__((weak)) int weakValue = 11;
inline int inlineValue = 12;

int main() {
    auto [y, m, d] = ymd(800);
    printf("%d %d %d\n", y, m, d);
    auto [i, f, s] = three();
    printf("%d %g %s\n", i, f, s);
    std::pair<int, std::string> p(3, "three");
    auto &[n, name] = p;
    name += "!";
    n = 4;
    printf("%d %s\n", p.first, p.second.c_str());
    const auto [cn, cname] = p;
    printf("%d %s\n", cn, cname.c_str());
    std::map<std::string, int> counts;
    counts["a"] = 1; counts["b"] = 2; counts["c"] = 3;
    for (auto &[key, value] : counts) value *= 10;
    for (const auto &[key, value] : counts) printf("%s=%d\n", key.c_str(), value);
    std::vector<Pt> pts = { {1, 2}, {3, 4} };
    int sum = 0;
    for (auto [px, py] : pts) sum += px * py;
    printf("sum %d\n", sum);
    for (auto &[px, py] : pts) { px += 1; py += 1; }
    printf("%d %d\n", pts[1].x, pts[1].y);
    int arr[3] = {5, 6, 7};
    auto [a0, a1, a2] = arr;
    auto &[r0, r1, r2] = arr;
    r1 = 60;
    printf("%d %d %d %d\n", a0, a1, a2, arr[1]);
    std::array<int, 2> sa = {{8, 9}};
    auto [s0, s1] = sa;
    printf("%d %d\n", s0, s1);
    auto t = std::make_tuple(1, 2);
    auto &[t0, t1] = t;
    t1 = 22;
    printf("%d\n", std::get<1>(t));
    std::vector<std::pair<int, int>> vp = { {1, 10}, {2, 20} };
    for (const auto &[k, v] : vp) printf("%d:%d ", k, v);
    printf("\n");
    C c = made(2);
    auto [q0, q1] = twoInts();
    printf("%d %d %d\n", c.s, q0, q1);
    YMD both[2] = { {7, 8, 9}, {1, 1, 1} };
    printf("%d %d %d\n", both[0].d, weakValue, inlineValue);
    return 0;
}
