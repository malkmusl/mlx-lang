// ANGLE forms 2: constexpr evaluation assigning through calls returning
// references (std::array's operator[] in a constexpr constructor), the
// members of constant globals, a constexpr object's const member
// functions, default template template arguments, conversion function
// templates, functions' addresses (member function pointers) in
// constexpr objects and constexpr static locals; std::thread on a
// member function.
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <array>
#include <limits>
#include <thread>
#include <type_traits>
#include <initializer_list>

namespace p_refplace {
struct A {
    constexpr A(int x) : data{} { data[1] = x; }
    std::array<int, 3> data;
};
constexpr A a(5);
static_assert(a.data[1] == 5);
struct C {
    constexpr C(std::initializer_list<int> list) : data{} {
        int i = 0;
        for (const int *it = list.begin(); it != list.end(); ++it) data[i++] = *it;
    }
    std::array<int, 3> data;
};
constexpr C c = {7, 8};
static_assert(c.data[1] == 8);
int run() { printf("%d %d %d\n", a.data[1], c.data[0], c.data[1]); return 0; }
}

namespace p_constobject {
struct M {
    int data[3];
    int &operator[](int i) { return data[i]; }
    constexpr const int &operator[](int i) const { return data[i]; }
    int which() { return 1; }
    constexpr int which() const { return 2; }
};
constexpr M m = {{4, 5, 6}};
static_assert(m[1] == 5);
static_assert(m.which() == 2);
int run() {
    M n = {{1, 2, 3}};
    n[0] = 7;
    printf("%d %d %d %d\n", m[2], m.which(), n.which(), n[0]);
    return 0;
}
}

namespace p_templatedefault {
template <typename L, typename R> struct IsGreater { static constexpr bool Test(const L lhs, const R rhs) { return lhs > rhs; } };
template <template <typename, typename> class C, typename L, typename R>
constexpr bool SafeCompare(const L lhs, const R rhs) { return C<L, R>::Test(lhs, rhs); }
template <typename Dst, template <typename> class Bounds = std::numeric_limits, typename Src>
constexpr bool InRange(Src value) {
    static_assert(Bounds<Dst>::lowest() < Bounds<Dst>::max(), "");
    return value >= Bounds<Dst>::lowest() && value <= Bounds<Dst>::max();
}
int run() {
    printf("%d %d %d\n", (int)SafeCompare<IsGreater, int, long>(3, 2L), (int)InRange<unsigned char>(300), (int)InRange<short>(300));
    return 0;
}
}

namespace p_conversiontemplate {
template <typename Dst, typename Src> struct Contained { static const bool value = sizeof(Dst) >= sizeof(Src); };
template <typename T> class Strict {
  public:
    constexpr Strict(T v) : value_(v) {}
    template <typename Dst, typename std::enable_if<Contained<Dst, T>::value>::type * = nullptr>
    constexpr operator Dst() const { return static_cast<Dst>(value_); }
  private:
    T value_;
};
static long twice(long v) { return v * 2; }
int run() {
    Strict<int> s(21);
    long l = s;
    double d = s;
    printf("%ld %ld %g\n", l, twice(s), d);
    return 0;
}
}

namespace p_handlers {
class State {
  public:
    int a(int x) { return x + 1; }
    int b(int x) { return x * 2; }
    int c(int x) { return x - 3; }
    using Handler = int (State::*)(int);
    using Handlers = std::array<Handler, 3>;
    static constexpr Handlers Make() {
        Handlers handlers{};
        handlers[0] = &State::a;
        handlers[1] = &State::b;
        handlers[2] = &State::c;
        return handlers;
    }
    int run(size_t which, int x) {
        static constexpr Handlers handlers = Make();
        constexpr auto missing = []() constexpr {
            for (auto handler : handlers) {
                if (handler == nullptr) return true;
            }
            return false;
        };
        static_assert(!missing(), "a handler is missing");
        return (this->*handlers[which])(x);
    }
};
using Arr = std::array<long, 3>;
static constexpr Arr Make() {
    Arr a{};
    a[0] = 4;
    a[1] = 5;
    a[2] = 6;
    return a;
}
int get(int i) {
    static constexpr Arr h = Make();
    static_assert(h[1] == 5);
    return (int)h[i];
}
int run() {
    State s;
    printf("%d %d %d %d %d\n", s.run(0, 10), s.run(1, 10), s.run(2, 10), get(0), get(2));
    return 0;
}
}

namespace p_threadmember {
struct Pool {
    int total = 0;
    void work(int n) { total += n; }
};
int run() {
    Pool pool;
    std::thread t(&Pool::work, &pool, 7);
    t.join();
    printf("%d\n", pool.total);
    return 0;
}
}

int main() {
    p_refplace::run();
    p_constobject::run();
    p_templatedefault::run();
    p_conversiontemplate::run();
    p_handlers::run();
    p_threadmember::run();
    return 0;
}
