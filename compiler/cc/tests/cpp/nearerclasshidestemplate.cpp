// A class declared in a namespace hides a class template of the same
// name brought into the file scope by a using-declaration after it
// (WebCore::Range, only declared in FrameSelection.h, beside WTF's
// `using WTF::Range;`): `Range` there names the class, not the
// template's deduced instance.
#include <cstdio>
#include <type_traits>
namespace N { class Range; }
namespace W {
template<typename T> class Range {
public:
    Range(T b, T e) : b(b), e(e) {}
    T b, e;
};
}
using W::Range;
namespace N {
template<typename T, typename U = int> struct P { T* p; };
class Range;
struct S { P<Range> m; P<Range, P<Range>> n; Range* r; };
class Range { public: int v = 5; };
static_assert(std::is_same_v<decltype(S::m), P<N::Range>>);
static_assert(std::is_same_v<decltype(S::n), P<N::Range, P<N::Range>>>);
static_assert(std::is_same_v<decltype(S::r), N::Range*>);
}
int main() {
    N::Range r;
    N::S s { { &r }, { &r }, &r };
    Range w(1, 4);
    std::printf("%d %d %d %d\n", s.m.p->v, s.n.p->v, s.r->v, w.e - w.b);
    return 0;
}
