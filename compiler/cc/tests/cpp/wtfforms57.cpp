// Type traits by the object's layout: unique object representations (padding, floating point, bit-fields), standard layout (bases with data, virtual functions), aggregates, trivial copies of polymorphic classes and classes with destructors.
#include <type_traits>
#include <stdio.h>
#include <stdint.h>

namespace p_traits3 {
struct P1 { char c; int i; };
struct P2 { int a; int b; };
struct P3 { double d; char c; };
struct B1 { int a; };
struct D1 : B1 { int b; };
struct V1 { virtual void f() {} int x; };
struct C1 { C1(const C1&) {} int x; };
struct C2 { C2(const C2&) = default; int x; };
struct M1 { C1 m; };
struct S1 { int a; private: int b; };
struct E1 {};
struct D2 : E1 { int x; };
struct Dt { ~Dt() {} };
struct Dm { Dt d; };
struct Ab { virtual void f() = 0; };
struct Fl { float f; };
struct Bf { int a : 3; int b : 29; };
enum En { X };
#define T(tr, ty) printf("%d", (int)std::tr<ty>);
#define ROW(tr) T(tr,P1) T(tr,P2) T(tr,P3) T(tr,B1) T(tr,D1) T(tr,V1) T(tr,C1) T(tr,C2) T(tr,M1) T(tr,E1) T(tr,D2) T(tr,Dt) T(tr,Dm) T(tr,Fl) T(tr,Bf) T(tr,En) T(tr,int) T(tr,float) T(tr,int*) printf(" " #tr "\n");
int run() {
    ROW(has_unique_object_representations_v)
    ROW(is_trivially_copyable_v)
    ROW(is_standard_layout_v)
    ROW(is_trivial_v)
    ROW(is_trivially_destructible_v)
    ROW(is_trivially_copy_constructible_v)
    ROW(is_trivially_default_constructible_v)
    ROW(is_polymorphic_v)
    ROW(is_empty_v)
    ROW(is_aggregate_v)
    ROW(is_pod_v)
    printf("%d %d\n", (int)std::is_abstract_v<Ab>, (int)std::is_abstract_v<V1>);
}
}

int main() {
    p_traits3::run();
    return 0;
}
