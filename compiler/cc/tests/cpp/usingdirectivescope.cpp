// A using-directive of a namespace applies in that namespace only: `using
// namespace D;` in J::F does not bring D's class template Dominators into
// J::B, whose own class Dominators is declared there (JSC's unified FTL
// bundle: FTLCapabilities.cpp's `using namespace DFG;` before B3Procedure.h).
// A block's directive reaches a lambda in a lambda (B3's LowerToAir); one
// in an unnamed namespace is in force around it (Skia's `namespace {
// using namespace skgpu::tess; }` before `namespace skgpu::ganesh`).
#include <stdio.h>
namespace W { template<typename T> class Dominators { public: int x = 1; }; }
namespace J { namespace D { template<typename C> class Dominators : public W::Dominators<C> { }; } }
namespace J { namespace F { using namespace D; int g() { Dominators<int> d; return d.x; } } }
namespace J { namespace B {
class Dominators;
class Proc {
public:
    Dominators& dominators();
    Dominators* m;
};
class Dominators { public: int y = 5; };
Dominators& Proc::dominators() { return *m; }
namespace Air { enum Opcode { Oops, MultiplyAdd32, MultiplyAdd64 }; }
class Lower {
public:
    int lower(int t)
    {
        using namespace Air;
        auto outer = [&]() -> bool {
            auto inner = [&](int a, int b) -> bool {
                Air::Opcode op = t ? MultiplyAdd32 : MultiplyAdd64;
                return op == MultiplyAdd32 && a + b == 3;
            };
            return inner(1, 2);
        };
        return outer();
    }
};
} }
namespace sk { namespace tess { constexpr static float kMax = 1024; } }
namespace { using namespace sk::tess; }
namespace sk { namespace ganesh { float f() { return kMax; } } }
int main()
{
    J::B::Dominators d;
    J::B::Proc p;
    p.m = &d;
    J::B::Lower l;
    printf("%d %d %d %g\n", p.dominators().y, J::F::g(), l.lower(1), sk::ganesh::f());
    return 0;
}
