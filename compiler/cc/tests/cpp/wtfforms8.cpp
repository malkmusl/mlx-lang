// WTF forms: specializations declared then defined, final classes, bool comparisons, explicit conversions, member classes made when needed
#include <stdio.h>
#include <stdint.h>
#include <type_traits>
#include <memory>

namespace p_sp1 {
namespace N {
template<typename T> T rnd() = delete;
template<> uint8_t rnd<uint8_t>();
template<> unsigned rnd<unsigned>();
}
int use() { return N::rnd<uint8_t>() + N::rnd<unsigned>(); }
namespace N {
template<> uint8_t rnd<uint8_t>() { return 4; }
template<> unsigned rnd<unsigned>() { return 30; }
}
void run() { printf("%d\n", use()); }
}

namespace p_fin1 {
struct A final { int x; };
struct B { };
template<class T> struct C final : B { T t; };
void run() { printf("%d %d %d %d\n", std::is_final_v<A>, std::is_final_v<B>, std::is_final_v<C<int>>, std::is_final_v<const A>); }
}

namespace p_cmp2 {
int f(int) { return 1; } int f(bool) { return 2; }
void run() { int a = 3, b = 4; long x = (a < b) + 10; printf("%d %d %ld %d %d\n", a == b, a < b, x, f(a == 3), (a < b) * 5); }
}

namespace p_ex1 {
struct P { explicit operator bool() const { return p != 0; } int* p; };
struct E { explicit E(int v) : v(v) {} int v; };
int f(E) { return 1; } int f(long) { return 2; }
void run() { int x = 1; P a{&x}, b{nullptr}; int n = 0; if (a) n++; if (!b) n += 10; bool c = static_cast<bool>(a); printf("%d %d %d %d\n", n, (int)c, a && !b, f(5)); }
}

namespace p_cv1 {
struct P { explicit operator bool() const { return true; } int* p; };
struct Q { operator bool() const { return true; } };
void run() { printf("%d %d %d %d\n", std::is_convertible<P, long double>::value, std::is_convertible<Q, long double>::value, std::is_convertible<P, bool>::value, std::is_constructible<bool, P>::value); }
}

namespace p_nest1 {
template<typename T> struct Box { T value; };
template<typename R> struct Promise {
    using AllType = Promise<Box<R>>;
    struct Producer { int id = 7; };
    struct AllProducer {
        std::unique_ptr<typename AllType::Producer> producer;
        int count() const { return producer ? producer->id : -1; }
    };
    struct Inner { R r; int twice() const { return 2 * (int)r; } };
    Inner inner { R(21) };
    int get() const { return inner.twice(); }
};
void run() {
    Promise<int> p;
    Promise<int>::AllProducer all;
    all.producer.reset(new Promise<Box<int>>::Producer);
    Promise<int>::Inner in { 5 };
    printf("%d %d %d\n", p.get(), all.count(), in.twice());
}
}

int main() {
    p_sp1::run();
    p_fin1::run();
    p_cmp2::run();
    p_ex1::run();
    p_cv1::run();
    p_nest1::run();
    return 0;
}
