// Variadic templates: parameter packs of function and class templates,
// sizeof..., expansion in calls, initializers and template arguments,
// a pack taken by a partial specialization (Tuple<T, Rest...>), fold
// expressions over the pack (left, right, with and without a start,
// empty), packs of references forwarded on.
#include <cstdio>
template <typename T> void show(T x) { printf("[%d]", (int)x); }
template <typename... Args> int count(Args... args) { return sizeof...(args) + sizeof...(Args); }
template <typename... Args> void showAll(Args... args) { int dummy[] = { 0, (show(args), 0)... }; (void)dummy; printf(" n=%d\n", (int)sizeof...(Args)); }
template <typename T, typename... Rest> T first(T x, Rest... rest) { return x + count(rest...); }
template <typename... Args> void forwardAll(const char *tag, Args &... args) { printf("%s", tag); showAll(args...); }
template <typename... Args> int sumList(Args... args) { int total = 0; int list[] = { 0, (total += args)... }; (void)list; return total; }
int part1() {
    printf("%d %d\n", count(), count(1, 2.0, 'c'));
    showAll(1, 2, 3);
    showAll();
    printf("%d\n", first(10, 'a', 2));
    int a = 4; char b = 5;
    forwardAll("fw:", a, b);
    printf("%d\n", sumList(1, 2, 3, 4));
    return 0;
}

template <typename... Ts> struct Tuple;
template <> struct Tuple<> { static int size() { return 0; } };
template <typename T, typename... Rest> struct Tuple<T, Rest...> {
    T head;
    Tuple<Rest...> tail;
    static int size() { return 1 + Tuple<Rest...>::size(); }
};
template <typename... Ts> struct Count { enum { value = sizeof...(Ts) }; };
template <typename A, typename B> struct Pair { A a; B b; };
template <typename... Args> Pair<Args...> makePair(Args... args) { Pair<Args...> p = { args... }; return p; }
template <typename T> T &forward(T &x) { return x; }
template <typename... Args> int sum(Args... args) { return (args + ... + 0); }
template <typename... Args> int sumLeft(Args... args) { return (0 + ... + args); }
template <typename... Args> bool all(Args... args) { return (... && args); }
template <typename... Args> bool any(Args... args) { return (args || ...); }
template <typename... Args> void print(Args... args) { ((printf("%d ", (int)args)), ...); printf("\n"); }
template <typename... Args> int callSum(Args &... args) { return sum(forward<Args>(args)...); }
int part2() {
    Tuple<int, char, double> t;
    t.head = 1; t.tail.head = 'x'; t.tail.tail.head = 2.5;
    printf("%d %c %g %d %d\n", t.head, t.tail.head, t.tail.tail.head, Tuple<int, char, double>::size(), Tuple<>::size());
    printf("%d %d\n", Count<>::value, Count<int, int, int>::value);
    Pair<int, char> p = makePair(7, 'q');
    printf("%d %c\n", p.a, p.b);
    printf("%d %d %d %d %d %d\n", sum(1, 2, 3), sum(), sumLeft(4, 5), all(true, 1, 2), all(), any(0, 0, 1));
    print(1, 2, 3);
    print();
    int a = 3, b = 4;
    printf("%d\n", callSum(a, b));
    return 0;
}

int main() { part1(); part2(); return 0; }
