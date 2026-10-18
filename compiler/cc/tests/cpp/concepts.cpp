// Concepts: concept definitions over traits and requires expressions,
// requires clauses on templates (after the parameter list and after the
// function parameters), constrained parameters (template <HasSize T>,
// template <SameAs<int> T>), overloads chosen by their constraints;
// trailing return types (-> int, -> decltype(x / 2)) and a deduced
// auto return type. (g++ -std=c++20 builds this one.)
#include <cstdio>
#include <type_traits>
template <class T> concept Integral = std::is_integral_v<T>;
template <class T> concept Pointer = std::is_pointer_v<T>;
template <class T> concept HasSize = requires(T t) { t.size(); { t.size() } -> Integral; };
template <class T, class U> concept SameAs = std::is_same_v<T, U>;
struct Bag { int n; int size() const { return n; } };
template <class T> requires Integral<T> const char *kind(T) { return "integral"; }
template <class T> requires Pointer<T> const char *kind(T) { return "pointer"; }
template <class T> const char *kind(T) requires (!Integral<T> && !Pointer<T>) { return "other"; }
template <HasSize T> int sizeOf(const T &t) { return t.size(); }
template <class T> int sizeOf(const T &) { return -1; }
template <SameAs<int> T> const char *exact(T) { return "int"; }
template <class T> const char *exact(T) { return "not int"; }
auto twice(int x) -> int { return x + x; }
auto plus(int a, int b) { return a + b; }
template <class T> auto half(T x) -> decltype(x / 2) { return x / 2; }
static_assert(Integral<int> && !Integral<double> && Pointer<int *> && HasSize<Bag> && !HasSize<int>, "concepts");
int main() {
    int a = 1;
    Bag b = { 4 };
    printf("%s %s %s %d %d %s %s %d %d %g\n", kind(1), kind(&a), kind(2.5), sizeOf(b), sizeOf(3), exact(1), exact('c'), twice(4), plus(2, 3), half(5.0));
    return 0;
}
