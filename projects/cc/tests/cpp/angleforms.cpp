// ANGLE forms: static_assert of a class's operator bool, value template
// parameters naming earlier ones (and their SFINAE, in and out of the
// class), explicit specializations giving the first arguments only,
// object pointers as template arguments, attributes after `enum class`,
// a parameter used twice as the more specialized template, members of
// anonymous unions in constructors' lists, value defaults over a
// pattern's parameters, `{{ {a, b}, {c, d} }}` for an initializer_list,
// `T &` over `const T &` for a variable, `thread_local x(v)` locals.
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <type_traits>
#include <utility>
#include <array>
#include <initializer_list>
#include <atomic>

namespace p_assertbool {
struct R { int a; int b; R() : a(1) {} };
static_assert(std::is_trivially_copyable<R>(), "x");
static_assert(std::is_integral<int>(), "y");
int run() { printf("%d\n", (int)std::is_integral<long>()); return 0; }
}

namespace p_valuesfinae {
template <size_t N> class Bits {
  public:
    template <size_t S, std::enable_if_t<(S < 64), int> = 0>
    void initFromValue(uint64_t value);
    template <size_t S, std::enable_if_t<(S >= 64), int> = 0>
    void initFromValue(uint64_t value);
    template <size_t S, std::enable_if_t<(S < 64), int> = 0>
    void inlineValue(uint64_t value) { v = value & ((1ull << S) - 1); }
    template <size_t S, std::enable_if_t<(S >= 64), int> = 0>
    void inlineValue(uint64_t value) { v = value + N; }
    uint64_t v = 0;
};
template <size_t N>
template <size_t S, std::enable_if_t<(S < 64), int>>
void Bits<N>::initFromValue(uint64_t value) { v = value & ((1ull << S) - 1); }
template <size_t N>
template <size_t S, std::enable_if_t<(S >= 64), int>>
void Bits<N>::initFromValue(uint64_t value) { v = value + N; }

template <size_t S, std::enable_if_t<(S < 64), int> = 0> int f() { return 1; }
template <size_t S, std::enable_if_t<(S >= 64), int> = 0> int f() { return 2; }

int run() {
    Bits<32> b;
    b.initFromValue<8>(0x1234);
    printf("%llx ", (unsigned long long)b.v);
    b.initFromValue<64>(0x1234);
    printf("%llx ", (unsigned long long)b.v);
    b.inlineValue<4>(0x1234);
    printf("%llx ", (unsigned long long)b.v);
    b.inlineValue<70>(0x1234);
    printf("%llx %d %d\n", (unsigned long long)b.v, f<3>(), f<100>());
    return 0;
}
}

namespace p_partialspec {
enum class Binding { Array, Element, Other };
template <typename E, typename From>
typename std::enable_if<std::is_enum<E>::value, E>::type Pack(From from) { return static_cast<E>(from); }
template <typename E, typename From>
typename std::enable_if<!std::is_enum<E>::value, E>::type Pack(From from) { return E(from) + 1; }
template <>
inline Binding Pack<Binding>(unsigned from) {
    if (from == 0x8892) return Binding::Array;
    if (from == 0x8893) return Binding::Element;
    return Binding::Other;
}
int run() {
    printf("%d %d %d %d\n", (int)Pack<Binding>(0x8892u), (int)Pack<Binding>(0x8893u), (int)Pack<Binding>(2), Pack<int>(41));
    return 0;
}
}

namespace p_objectarg {
constexpr const unsigned int kSize3[1] = {3};
constexpr const unsigned int kSize7[1] = {7};
struct Name { char text[4]; };
constexpr Name build(int b, const unsigned int *sizes) {
    Name n = {};
    n.text[0] = (char)('a' + b);
    n.text[1] = 'x';
    n.text[2] = (char)('0' + sizes[0]);
    n.text[3] = 0;
    return n;
}
template <int B, const unsigned int *sizes, size_t count>
static constexpr Name kName = build(B, sizes);
template <int B, const unsigned int *sizes, size_t count>
const char *get() {
    static_assert(sizes[0] < 10, "single digit");
    return kName<B, sizes, count>.text;
}
int run() { printf("%s %s %s\n", get<1, kSize3, 1>(), get<2, kSize7, 1>(), get<1, kSize3, 1>()); return 0; }
}

namespace p_enumattr {
enum class [[nodiscard]] Result { Continue, Stop };
Result step(int n) { return n > 2 ? Result::Stop : Result::Continue; }
int run() { printf("%d %d\n", (int)step(1), (int)step(5)); return 0; }
}

namespace p_twice {
template <typename I, typename J, typename P> int eq(I, I, J, P) { return 1; }
template <typename I, typename J> int eq(I, I, J, J) { return 2; }
template <typename T> int g(T, T) { return 1; }
template <typename T> int g(T *, T *) { return 2; }
int run() {
    int a[2] = {1, 2};
    const int *p = a;
    printf("%d %d %d %d\n", eq(p, p, p, p), eq(p, p, p, 3), g(1, 2), g(p, p));
    return 0;
}
}

namespace p_anonunion {
template <typename T> struct Color {
    Color();
    constexpr Color(T r, T g, T b, T a);
    T red, green, blue, alpha;
};
template <typename T> Color<T>::Color() : Color(0, 0, 0, 0) {}
template <typename T> constexpr Color<T>::Color(T r, T g, T b, T a) : red(r), green(g), blue(b), alpha(a) {}
struct Generic {
    Generic(const Color<float> &c) : colorF(c), type(0) {}
    Generic(const Color<int> &c) : colorI(c), type(1) {}
    union {
        Color<float> colorF;
        Color<int> colorI;
    };
    int type;
};
int run() {
    Generic a(Color<float>(1.5f, 2, 3, 4));
    Generic b(Color<int>(5, 6, 7, 8));
    printf("%g %d %d %d\n", a.colorF.red, a.type, b.colorI.alpha, b.type);
    return 0;
}
}

namespace p_patterndefault {
template <typename E> constexpr size_t EnumSize() { return static_cast<typename std::underlying_type<E>::type>(E::EnumCount); }
template <typename E, typename T, size_t MaxSize = EnumSize<E>()>
class Map {
  public:
    T &operator[](E e) { return data[static_cast<size_t>(e)]; }
    size_t size() const { return MaxSize; }
    std::array<T, MaxSize> data{};
};
enum class Shader : unsigned char { Vertex, Fragment, Compute, EnumCount };
template <typename E, typename T> size_t count(const Map<E, T> &m) { return m.size(); }
int run() {
    Map<Shader, int> m;
    m[Shader::Fragment] = 4;
    printf("%zu %d %zu\n", m.size(), m[Shader::Fragment], count(m));
    return 0;
}
}

namespace p_nestedlist {
enum class Component : uint8_t { Float, Int, UInt, EnumCount };
template <typename E, typename T> class Map {
  public:
    using InitPair = std::pair<E, T>;
    constexpr Map(std::initializer_list<InitPair> init) : data{} {
        for (const InitPair *it = init.begin(); it != init.end(); ++it) data[static_cast<size_t>(it->first)] = it->second;
    }
    constexpr const T &operator[](E e) const { return data[static_cast<size_t>(e)]; }
  private:
    std::array<T, 3> data;
};
constexpr Map<Component, uint32_t> kMasks = {{
    {Component::Float, 0x10001},
    {Component::Int, 0x00001},
}};
const Map<Component, bool> gModes = {{{Component::Int, true}, {Component::UInt, true}}};
int run() {
    printf("%x %x %d %d\n", kMasks[Component::Float], kMasks[Component::Int], gModes[Component::Float], gModes[Component::UInt]);
    return 0;
}
}

namespace p_refbinding {
template <typename T> static constexpr int pick(const T &) noexcept { return 1; }
template <typename T> static constexpr int pick(T &) noexcept { return 2; }
struct Dir { int id; };
template <class T> int g(T *) { return 1; }
template <class T> int g(T &) { return 2; }
int run() {
    int value = 0;
    const int c = 1;
    Dir a { 1 };
    Dir *p = &a;
    printf("%d %d %d %d %d %d\n", pick(value), pick(c), pick(3), g(a), g(&a), g(p));
    return 0;
}
}

namespace p_threadlocal {
uint64_t next() {
    static std::atomic<uint64_t> serial;
    thread_local uint64_t id(++serial);
    return id;
}
int run() { printf("%llu %llu\n", (unsigned long long)next(), (unsigned long long)next()); return 0; }
}

int main() {
    p_assertbool::run();
    p_valuesfinae::run();
    p_partialspec::run();
    p_objectarg::run();
    p_enumattr::run();
    p_twice::run();
    p_anonunion::run();
    p_patterndefault::run();
    p_nestedlist::run();
    p_refbinding::run();
    p_threadlocal::run();
    return 0;
}
