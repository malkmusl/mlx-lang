// Argument-dependent lookup through a variadic template's arguments: the
// namespaces of every element of the pack (and of their template
// arguments) are the argument's, as WebCore's `switchOn(m_data, ...)` of a
// Variant<Tuple, ProcessQualified<ObjectIdentifier<...>>> finds
// WTF::switchOn.
#include <cstdio>
#include <utility>

namespace mp {
template<class... T> struct variant { int index = 0; };
template<class T> struct box { int index = 1; };
}
namespace W {
template<class V, class... F> int switchOn(V&& v, F&&... f) { return v.index + sizeof...(F); }
template<class T> struct ObjectIdentifier { T* p; };
struct OpaqueType { };
}
namespace C {
template<class T> struct PQ { T t; };
using Opaque = W::ObjectIdentifier<W::OpaqueType>;
struct S {
    struct Tuple { int a; };
    mp::variant<Tuple, PQ<Opaque>> m;
    int f() const { return switchOn(m, [](const Tuple&) { return 1; }, [](const PQ<Opaque>&) { return 2; }); }
};
}

int main()
{
    C::S s;
    mp::box<W::OpaqueType> b;
    mp::variant<W::OpaqueType> v;
    mp::variant<int, W::OpaqueType> w;
    std::printf("%d %d %d %d\n", s.f(), switchOn(b), switchOn(v), switchOn(w, 1, 2));
    return s.f();
}
