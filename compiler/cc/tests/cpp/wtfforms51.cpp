// Lambdas in a generic lambda's instance (PropertyTable's withIndexVector / forEachProperty): the class's members and member templates through the closures' captured this; explicit captures of the outer closure's captures
#include <stdio.h>
#include <stdint.h>
#include <utility>

namespace p_lam4 {
struct Table {
    long base = 1000;
    uintptr_t vec = 0;
    template<typename Index, typename Entry> void reinsert(Index* iv, Entry* t, const long& e) { iv[0] = (Index)(base + e); (void)t; }
    template<typename F> static auto withIndexVector(uintptr_t v, F&& f) -> decltype(auto) { if (v & 1) return f((uint8_t*)(v & ~(uintptr_t)1)); return f((uint32_t*)v); }
    template<typename F> auto withIndexVector(F&& f) const -> decltype(auto) { return withIndexVector(vec, std::forward<F>(f)); }
    template<typename F> void forEach(F&& f) const { long items[3] = { 1, 2, 3 }; for (long &i : items) f(i); }
    long copyFrom(const Table& other) {
        static uint32_t storage[4];
        vec = (uintptr_t)storage;
        long sum = 0;
        withIndexVector([&](auto* vector) {
            other.forEach([&](auto& entry) {
                reinsert(vector, (int *)0, entry);
                sum += vector[0];
            });
        });
        return sum;
    }
};
int run() { Table a, b; b.base = 7; printf("%ld\n", a.copyFrom(b)); return 0; }
}

namespace p_lam5 {
struct T {
    long base = 1000;
    template<typename X> long tm(X x) { return base + x; }
    long pm(long x) { return base + x; }
    template<typename F> void each(F&& f) const { for (long i = 1; i <= 3; i++) f(i); }
    long a() { long s = 0; [&](auto v) { each([&](auto& e) { s += tm(e) + v; }); }(10); return s; }
    long b() { long s = 0; [&](auto v) { each([&](long& e) { s += tm(e) + v; }); }(10); return s; }
    long c() { long s = 0; [&]() { each([&](auto& e) { s += tm(e); }); }(); return s; }
    long d() { long s = 0; [&](auto v) { each([&](auto& e) { s += pm(e) + v; }); }(10); return s; }
    long e() { long s = 0; [&](auto v) { each([&](auto& e) { s += base + e + v; }); }(10); return s; }
    long f() { long s = 0; [&](auto v) { each([&](auto& e) { s += this->tm(e) + v; }); }(10); return s; }
};
int run() { T t; printf("%ld %ld %ld %ld %ld %ld\n", t.a(), t.b(), t.c(), t.d(), t.e(), t.f()); return 0; }
}

namespace p_lam7 {
struct T {
    long base = 1000;
    template<typename X> long tm(X x) { return base + x; }
    template<typename F> void each(F&& f) const { for (long i = 1; i <= 2; i++) f(i); }
    long three() { long s = 0; long k = 5; [&](auto v) { each([&](auto a) { each([&](auto b) { s += tm(a * b) + v + k; }); }); }(10); return s; }
    long byValue() { long k = 7; long s = 0; [=, &s](auto v) { each([=, &s](auto a) { s += a + v + k + base; }); }(1); return s; }
    long explicitThis() { long s = 0; [this, &s](auto v) { each([this, &s, v](auto a) { s += this->base + a + v; }); }(1); return s; }
    long mixed() { long s = 0; auto f = [&](auto v) { return [&, v](long w) { return tm(v + w); }; }; s = f(3)(4); return s; }
};
int run() { T t; printf("%ld %ld %ld %ld\n", t.three(), t.byValue(), t.explicitThis(), t.mixed()); return 0; }
}

int main() {
    p_lam4::run();
    p_lam5::run();
    p_lam7::run();
    return 0;
}
