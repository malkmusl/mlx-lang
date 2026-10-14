// constexpr: functions computed at compile time when called with
// constants (recursion, loops, switch, break, compound assignments,
// static member functions), static constexpr data members, variable
// templates (with an explicit specialization), if constexpr choosing a
// branch without reading the other, static_assert over them; the same
// functions called with runtime values at run time.
#include <cstdio>
constexpr int square(int x) { return x * x; }
constexpr int factorial(int n) { return n <= 1 ? 1 : n * factorial(n - 1); }
constexpr unsigned long sumTo(unsigned long n) { unsigned long total = 0; for (unsigned long i = 1; i <= n; ++i) total += i; return total; }
constexpr int collatz(int n) { int steps = 0; while (n != 1) { if (n % 2 == 0) n /= 2; else n = 3 * n + 1; steps++; } return steps; }
constexpr bool isPowerOfTwo(unsigned v) { return v != 0 && (v & (v - 1)) == 0; }
constexpr int pick(int k) { switch (k) { case 1: return 10; case 2: return 20; default: return -1; } }
constexpr int firstOver(int limit) { int i = 0; do { i += 3; if (i > 100) break; } while (i < limit); return i; }
struct Bits { static constexpr int count(int v) { int n = 0; while (v) { n += v & 1; v >>= 1; } return n; } static constexpr int width = 8; };
template <int N> struct Fib { static constexpr int value = Fib<N - 1>::value + Fib<N - 2>::value; };
template <> struct Fib<0> { static constexpr int value = 0; };
template <> struct Fib<1> { static constexpr int value = 1; };
template <typename T> constexpr bool isSmall = sizeof(T) < 4;
template <typename T> constexpr int kindOf = 0;
template <> constexpr int kindOf<int> = 1;
template <typename T> int describe(T) {
    if constexpr (isSmall<T>) { return 1; } else if constexpr (kindOf<T> == 1) { return 2; } else { return 3; }
}
static_assert(square(7) == 49, "square");
static_assert(factorial(5) == 120, "factorial");
static_assert(Fib<10>::value == 55, "fib");
static_assert(Bits::count(0xff) == 8 && Bits::width == 8, "bits");
static_assert(isSmall<char> && !isSmall<double>, "small");
int main() {
    char table[square(3)];
    int cells[sumTo(4)];
    printf("%lu %lu %d %d %d %d %d %d\n", (unsigned long)sizeof(table), (unsigned long)sizeof(cells), collatz(27), isPowerOfTwo(64), isPowerOfTwo(65), pick(2), pick(9), firstOver(50));
    printf("%d %d %d %d\n", describe('c'), describe(1), describe(2.0), Fib<12>::value);
    int runtime = square(collatz(7));
    printf("%d\n", runtime);
    return 0;
}
