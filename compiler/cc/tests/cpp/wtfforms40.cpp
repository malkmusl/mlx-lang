// WTF/JSC forms: auto results from const references, overloaded members by their address, static data members of class templates, function template instances by their address
#include <stdio.h>
#include <type_traits>
#include <cstddef>

namespace p_dt2 {
struct Str { int n = 1; };
struct A { const Str& message() const { static Str s; return s; } };
struct B { Str message() const { return Str{}; } };
int run() {
    auto l = [](const auto& e) { return e.message(); };
    A a; B b;
    using RA = decltype(l(a));
    using RB = decltype(l(b));
    printf("%d %d %d\n", (int)std::is_same<RA, Str>::value, (int)std::is_same<RB, Str>::value, (int)std::is_same<RA, RB>::value);
    auto f = []() { const Str& r = Str{}; return r; };
    printf("%d\n", (int)std::is_same<decltype(f()), Str>::value);
    return 0;
}
}

namespace p_vc1 {
struct Cell {};
struct ASV { int k = 0; };
struct SV : ASV {};
struct Table { void (*a)(Cell*, SV&); void (*b)(Cell*, ASV&); };
template<typename A> struct View {
    static void visitChildren(Cell*, ASV&);
    static void visitChildren(Cell*, SV&);
    template<typename V> static void impl(Cell*, V& v) { v.k += sizeof(A); }
    static const Table s_table;
};
template<typename A> void View<A>::visitChildren(Cell* c, ASV& v) { v.k += 100; impl(c, v); }
template<typename A> void View<A>::visitChildren(Cell* c, SV& v) { impl(c, v); }
template<typename A> const Table View<A>::s_table = { &View<A>::visitChildren, &View<A>::visitChildren };
int run() { Cell c; ASV a; SV s; View<int>::s_table.a(&c, s); View<int>::s_table.b(&c, a); printf("%d %d\n", s.k, a.k); return 0; }
}

namespace p_vc2 {
struct Cell {};
struct ASV { int k = 0; };
struct SV : ASV {};
struct Table { void (*a)(Cell*, SV&); void (*b)(Cell*, ASV&); };
template<typename A> struct View {
    static void visitChildren(Cell*, ASV&);
    static void visitChildren(Cell*, SV&);
    template<typename V> static void impl(Cell*, V& v) { v.k += sizeof(A); }
};
template<typename A> void View<A>::visitChildren(Cell* c, ASV& v) { v.k += 100; impl(c, v); }
template<typename A> void View<A>::visitChildren(Cell* c, SV& v) { impl(c, v); }
int run() { Cell c; ASV a; SV s; Table t = { &View<int>::visitChildren, &View<int>::visitChildren }; t.a(&c, s); t.b(&c, a); printf("%d %d\n", s.k, a.k); return 0; }
}

namespace p_sd1 {
struct Info { const char* name; int size; };
template<typename A> struct View { static const Info s_info; static int count; };
template<typename A> const Info View<A>::s_info = { "view", (int)sizeof(A) };
template<typename A> int View<A>::count = 7;
int run() { View<long>::count++; printf("%s %d %d\n", View<long>::s_info.name, View<long>::s_info.size, View<long>::count); return 0; }
}

namespace p_fp1 {
template<std::size_t I> int dispatch(int x) { return x * 10 + (int)I; }
template<std::size_t... Is> struct Table { int (*fns[sizeof...(Is)])(int) = { &dispatch<Is>... }; };
struct H { template<std::size_t I> static int disp(int x) { return x + (int)I; } static constexpr auto get() { return &disp<2>; } };
int run() {
    Table<0, 1, 2> t;
    auto f = &dispatch<5>;
    printf("%d %d %d %d\n", t.fns[1](3), t.fns[2](4), f(1), H::get()(7));
    return 0;
}
}

int main() {
    p_dt2::run();
    p_vc1::run();
    p_vc2::run();
    p_sd1::run();
    p_fp1::run();
    return 0;
}
