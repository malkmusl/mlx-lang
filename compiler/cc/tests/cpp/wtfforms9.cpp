// WTF forms: friends after specifiers and attributes, this through nested lambdas, constructor templates defined outside their class, old constexpr member definitions
#include <stdio.h>
#include <compare>
#include <utility>

namespace p_os1 {
namespace W {
template<typename E> class OptionSet {
public:
    constexpr OptionSet(E e) : m_storage(1u << static_cast<unsigned>(e)) { }
    constexpr OptionSet(unsigned s, int) : m_storage(s) { }
    constexpr explicit operator bool() const { return m_storage; }
    constexpr bool contains(E option) const { return containsAny(option); }
    constexpr bool containsAny(OptionSet optionSet) const { return !!(*this & optionSet); }
    constexpr friend OptionSet operator&(OptionSet lhs, OptionSet rhs) { return OptionSet(lhs.m_storage & rhs.m_storage, 0); }
    constexpr friend OptionSet operator|(OptionSet lhs, OptionSet rhs) { return OptionSet(lhs.m_storage | rhs.m_storage, 0); }
private:
    unsigned m_storage;
};
enum class P { A, B, C };
}
void run() { W::OptionSet<W::P> t = W::P::B; W::OptionSet<W::P> u = t & W::OptionSet<W::P>(W::P::B); printf("%d\n", u.contains(W::P::B)); W::OptionSet<W::P> s = W::OptionSet<W::P>(W::P::A) | W::P::C; printf("%d %d %d\n", s.contains(W::P::A), s.contains(W::P::B), s.contains(W::P::C)); }
}

namespace p_afr {
namespace W {
class T final {
public:
    double v;
    friend bool operator==(const T&, const T&) = default;
    __attribute__((visibility("default"))) friend std::partial_ordering operator<=>(const T&, const T&);
    T now() const { return T { v + 1 }; }
    explicit operator bool() const { return v != 0; }
};
std::partial_ordering operator<=>(const T& a, const T& b) { return a.v <=> b.v; }
}
void run() { W::T a { 1 }; printf("%d %d %d\n", a < a.now(), a.now() < a, a == a); }
}

namespace p_lam3 {
struct T {
    bool m_running = true;
    int m_count = 0;
    void start() {
        auto outer = [this]() {
            auto inner = [&](int k) { m_running = false; m_count += k; };
            inner(5);
        };
        outer();
    }
};
void run() { T t; t.start(); printf("%d %d\n", t.m_running, t.m_count); }
}

namespace p_lam6 {
struct T {
    int m_v = 1;
    int bump(int k) { m_v += k; return m_v; }
    int go() {

        auto a = [=, this]() {
            auto b = [&]() {
                auto c = [&]() { int r = bump(10); return r + this->m_v; };
                int x = c(); return x + m_v;
            };
            return b();
        };
        return a();
    }
};
void run() { T t; int r = t.go(); printf("%d %d\n", r, t.m_v); }
}

namespace p_ms3 {
typedef unsigned char L1;
template<typename T> struct Mix { T* p = nullptr; };
template<typename T, typename M = int> struct MS : Mix<T> { MS() = default; MS(MS&& o) { this->p = std::exchange(o.p, nullptr); } MS(const MS&) = delete; };
struct Fast { };
struct Shape { int n; Shape(int k) : n(k) {} };
struct S : private Shape {
    template<typename M> explicit S(MS<L1, M>);
    template<typename M> explicit S(MS<char16_t, M>);
    explicit S(unsigned) : Shape(3) {}
    int get() const { return n; }
};
template<typename M> inline S::S(MS<L1, M> s) : Shape(1) { }
template<typename M> inline S::S(MS<char16_t, M> s) : Shape(2) { }
template<typename C> MS<C, Fast> release() { return MS<C, Fast>(); }
void run() { S* a = new S(release<unsigned char>()); S* b = new S(release<char16_t>()); printf("%d %d\n", a->get(), b->get()); }
}

namespace p_nl2 {
struct U { int v; };
namespace q {
template<class T> class lim { public: static constexpr bool is_signed = true; };
template<> class lim<U> { public: static constexpr bool is_signed = false; static constexpr int digits = 128; };
constexpr bool lim<U>::is_signed;
constexpr int lim<U>::digits;
}
void run() { printf("%d %d\n", q::lim<U>::is_signed, q::lim<U>::digits); }
}

int main() {
    p_os1::run();
    p_afr::run();
    p_lam3::run();
    p_lam6::run();
    p_ms3::run();
    p_nl2::run();
    return 0;
}
