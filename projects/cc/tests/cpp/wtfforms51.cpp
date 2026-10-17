// Lambdas in a generic lambda's instance (PropertyTable's withIndexVector / forEachProperty): the class's members and member templates through the closures' captured this
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

int main() {
    p_lam4::run();
    p_lam5::run();
    return 0;
}
