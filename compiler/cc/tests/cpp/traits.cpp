// <type_traits>, <utility>, <limits>, <cstddef>, <initializer_list>:
// the traits answered by the __is_... builtins and partial
// specializations, the transformations, enable_if choosing overloads
// (SFINAE), common_type, integer_sequence, swap, exchange, pair with
// its constructor templates, byte.
#include <cstdio>
#include <type_traits>
#include <utility>
#include <limits>
#include <cstddef>
#include <initializer_list>
struct Plain { int a; };
struct WithCtor { WithCtor(int) {} };
struct Virtual { virtual ~Virtual() {} virtual int f() = 0; };
struct Derived : Virtual { int f() { return 1; } };
enum Color : unsigned char { RED, GREEN };
enum class Mode : short { A, B };
static_assert(std::is_same_v<int, int> && !std::is_same_v<int, long>, "same");
static_assert(std::is_same_v<std::remove_reference_t<int &&>, int>, "remove_reference");
static_assert(std::is_same_v<std::remove_cv_t<const int>, int>, "remove_cv");
static_assert(std::is_same_v<std::add_const_t<int>, const int>, "add_const");
static_assert(std::is_same_v<std::remove_pointer_t<int *>, int>, "remove_pointer");
static_assert(std::is_same_v<std::add_pointer_t<int &>, int *>, "add_pointer");
static_assert(std::is_same_v<std::decay_t<const int &>, int> && std::is_same_v<std::decay_t<int[3]>, int *>, "decay");
static_assert(std::is_same_v<std::conditional_t<true, int, char>, int> && std::is_same_v<std::conditional_t<false, int, char>, char>, "conditional");
static_assert(std::is_integral_v<int> && std::is_integral_v<bool> && !std::is_integral_v<float> && std::is_floating_point_v<double>, "integral");
static_assert(std::is_signed_v<int> && !std::is_signed_v<unsigned> && std::is_unsigned_v<unsigned char>, "signed");
static_assert(std::is_pointer_v<int *> && !std::is_pointer_v<int> && std::is_lvalue_reference_v<int &> && std::is_rvalue_reference_v<int &&>, "pointer");
static_assert(std::is_array_v<int[3]> && std::extent_v<int[7]> == 7 && std::rank_v<int[2][3]> == 2 && std::is_same_v<std::remove_extent_t<int[3]>, int>, "array");
static_assert(std::is_enum_v<Color> && std::is_enum_v<Mode> && !std::is_enum_v<int> && std::is_class_v<Plain> && !std::is_class_v<int>, "enum class");
static_assert(std::is_same_v<std::underlying_type_t<Color>, unsigned char> && std::is_same_v<std::underlying_type_t<Mode>, short>, "underlying");
static_assert(std::is_base_of_v<Virtual, Derived> && !std::is_base_of_v<Derived, Virtual> && std::is_polymorphic_v<Virtual> && std::is_abstract_v<Virtual> && !std::is_abstract_v<Derived>, "classes");
static_assert(std::is_trivially_copyable_v<Plain> && !std::is_trivially_copyable_v<Virtual> && std::is_empty_v<std::true_type>, "trivial");
static_assert(std::is_constructible_v<WithCtor, int> && !std::is_constructible_v<WithCtor> && std::is_default_constructible_v<Plain> && std::is_convertible_v<int, long> && !std::is_convertible_v<Plain, int>, "construct");
static_assert(std::is_void_v<void> && std::is_arithmetic_v<char> && std::is_scalar_v<int *> && std::is_object_v<Plain> && !std::is_object_v<int &>, "categories");
static_assert(std::numeric_limits<int>::max() == 2147483647 && std::numeric_limits<unsigned char>::max() == 255 && std::numeric_limits<int>::is_signed, "limits");
static_assert(std::is_same_v<std::common_type_t<int, long>, long>, "common");
static_assert(std::integer_sequence<int, 1, 2, 3>::size() == 3 && std::make_index_sequence<4>::size() == 4, "sequence");
static_assert(std::conjunction_v<std::true_type, std::true_type> && !std::conjunction_v<std::true_type, std::false_type> && std::disjunction_v<std::false_type, std::true_type> && std::negation_v<std::false_type>, "logic");
template <class T> typename std::enable_if<std::is_integral<T>::value, int>::type describe(T) { return 1; }
template <class T> typename std::enable_if<std::is_floating_point<T>::value, int>::type describe(T) { return 2; }
int main() {
    int a = 1, b = 2;
    std::swap(a, b);
    int old = std::exchange(a, 9);
    std::pair<int, const char *> p = std::make_pair(3, "x");
    std::byte bt = std::byte(0x0f);
    bt |= std::byte(0x30);
    std::initializer_list<int> none;
    printf("%d %d %d %d %s %d %d %d %d %lu\n", a, b, old, p.first, p.second, std::get<0>(p), std::to_integer<int>(bt), describe(1), describe(2.5), (unsigned long)none.size());
    return 0;
}
