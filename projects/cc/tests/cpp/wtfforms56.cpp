// Triviality traits see default member initializers, base classes and members with them: a Vector of such elements value-initializes new ones (JSC's BlockDirectoryBits segments).
#include <type_traits>
#include <array>
#include <stdio.h>
#include <stdint.h>

namespace p_triv {
struct A { A() = default; std::array<uint32_t, 4> m { }; };
struct B { int x = 0; };
struct C { int x; };
struct D { D() = default; int x; };
struct E { E() {} int x; };
struct F { int x; F() = default; };
struct G : A {};
struct H { A a; };
struct I { std::array<uint32_t, 4> m; };
int run() {
    printf("%d %d %d %d %d %d %d %d %d\n", std::is_trivially_default_constructible_v<A>, std::is_trivially_default_constructible_v<B>, std::is_trivially_default_constructible_v<C>,
        std::is_trivially_default_constructible_v<D>, std::is_trivially_default_constructible_v<E>, std::is_trivially_default_constructible_v<F>, std::is_trivially_default_constructible_v<G>, std::is_trivially_default_constructible_v<H>, std::is_trivially_default_constructible_v<I>);
    printf("%d %d %d %d\n", std::is_trivially_copyable_v<A>, std::is_standard_layout_v<A>, std::has_unique_object_representations_v<A>, std::is_trivial_v<B>);
    printf("%d %d %d\n", std::is_trivially_constructible_v<A>, std::is_trivially_constructible_v<B>, std::is_trivially_constructible_v<C>);
}
}

int main() {
    p_triv::run();
    return 0;
}
