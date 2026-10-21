// WTF/bmalloc forms: instances nested in their own template named lazily, unordered_map and unordered_set, 16-byte atomics (cmpxchg16b), nullptr to a concrete pointer parameter, array members braced in a constructor's list, designated initializers
#include <stdio.h>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace p_rc1 {
struct Owner { typedef Owner ViewType; unsigned w = 0x0f; unsigned word() const { return w; } Owner view() const { return *this; } };
template<typename V> struct Not { typedef Not ViewType; V v; unsigned word() const { return ~v.word(); } Not view() const { return *this; } };
template<typename Words> struct Impl {
  Words m;
  Impl<Not<typename Words::ViewType>> operator~() const { return Impl<Not<typename Words::ViewType>>{ Not<typename Words::ViewType>{ m.view() } }; }
  unsigned word() const { return m.word(); }
};
void run() {
  Impl<Owner> a{}; auto b = ~a; auto c = ~b; auto d = ~~~c;
  std::tuple<int, char, short, long, int, int, int, int, int, double> t(1, 'c', 2, 3L, 4, 5, 6, 7, 8, 9.5);
  printf("%x %x %x %x %g %d\n", a.word(), b.word(), c.word(), d.word(), std::get<9>(t), std::get<8>(t));
}
}

namespace p_um1 {
struct SH { size_t operator()(const std::string& s) const { size_t h = 5381; for (char c : s) h = h * 33 + (unsigned char)c; return h; } };
void run() {
  std::unordered_map<void*, size_t> sizes; int a, b; sizes[&a] = 4; sizes.emplace(&b, 8); sizes.insert({nullptr, 1});
  size_t total = 0; for (auto& kv : sizes) total += kv.second;
  auto it = sizes.find(&b); bool found = it != sizes.end(); sizes.erase(&a);
  std::unordered_map<std::string, int, SH> words; for (int i = 0; i < 100; i++) words[std::to_string(i % 37)] += i;
  std::unordered_set<int> s{1, 2, 3, 2, 1}; s.insert(10); s.erase(2);
  int sum = 0; for (int x : s) sum += x;
  printf("%zu %d %zu %zu %d %d %zu %d\n", total, (int)found, sizes.size(), words.size(), words["5"], sum, s.size(), (int)s.contains(3));
}
}

namespace p_cx16 {
typedef __uint128_t pair_t;
alignas(16) static pair_t slot;
void run() {
  slot = ((pair_t)7 << 64) | 9;
  pair_t expected = ((pair_t)7 << 64) | 9, desired = ((pair_t)1 << 64) | 2;
  int ok1 = __atomic_compare_exchange_n(&slot, &expected, desired, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  pair_t wrong = 5;
  int ok2 = __atomic_compare_exchange_n(&slot, &wrong, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  printf("%d %d %llu %llu %llu %llu\n", ok1, ok2, (unsigned long long)(slot >> 64), (unsigned long long)slot, (unsigned long long)(wrong >> 64), (unsigned long long)wrong);
}
}

namespace p_cx17 {
typedef __uint128_t pair_t;
alignas(16) static pair_t slot;
void run() {
  __atomic_store_n(&slot, ((pair_t)3 << 64) | 4, __ATOMIC_SEQ_CST);
  pair_t v = __atomic_load_n(&slot, __ATOMIC_RELAXED);
  __atomic_store_n(&slot, (pair_t)0, __ATOMIC_SEQ_CST);
  pair_t z = __atomic_load_n(&slot, __ATOMIC_RELAXED);
  printf("%llu %llu %llu\n", (unsigned long long)(v >> 64), (unsigned long long)v, (unsigned long long)z);
}
}

namespace p_np3 {
enum class P { A, B };
template<typename C, P p> double conv(const C* s, bool* ok, int& n) { n = (int)p; if (ok) *ok = true; return s[0]; }
template<typename T> int h(T*) { return 1; }
template<typename T> int h(T) { return 2; }
void run() { int n; const char s[] = "A"; double d = conv<char, P::B>(s, nullptr, n); printf("%g %d %d\n", d, n, h(nullptr)); }
}

namespace p_am1 {
struct Impl { int x; };
Impl nullImpl{7};
struct cpp_init_t {};
struct Table {
  unsigned bits[4];
  Impl* inst[3];
  int tail[2];
  constexpr Table(cpp_init_t) : bits { 5 }, inst { }, tail { 1, 2 } { for (int i = 0; i < 3; i++) if (i > 0) inst[i] = &nullImpl; }
};
void run() { Table t{cpp_init_t{}}; printf("%u %u %d %d %d %d\n", t.bits[0], t.bits[3], t.inst[0] == nullptr, t.inst[2]->x, t.tail[0], t.tail[1]); }
}

namespace p_di1 {
struct CP { unsigned payload; };
struct P3 { int a; int b; int c; };
struct H { CP arr[3]; H() : arr { } { for (int i = 0; i < 3; ++i) arr[i] = { .payload = 5u + i }; } };
void run() { H h; P3 p = { .a = 1, .c = 3 }; P3 q{ .b = 2 }; p = { .b = 9 }; printf("%u %u %d %d %d %d %d\n", h.arr[0].payload, h.arr[2].payload, p.a, p.b, p.c, q.b, q.c); }
}

int main() {
    p_rc1::run();
    p_um1::run();
    p_cx16::run();
    p_cx17::run();
    p_np3::run();
    p_am1::run();
    p_di1::run();
    return 0;
}
