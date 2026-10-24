// An aggregate made in parentheses of an object of another class that
// converts to it (a UniqueRef<T>'s operator T &): copied by its copy
// constructor, not its first member made of the object
// (makeUniqueRef<DynamicRangeLimitMixFunction>(mix) in WebCore's
// StyleDynamicRangeLimit).
#include <cstdio>
#include <utility>
struct Percentage {
    Percentage(double v) : v(v) { }
    double v;
};
struct Params { Percentage a; Percentage b; };
template<int C, typename T> struct Notation {
    T parameters;
    const T& operator*() const { return parameters; }
    operator const T&() const { return parameters; }
    operator T&() { return parameters; }
};
template<typename T> class UniqueRef {
public:
    UniqueRef(T& r) : m_ref(&r) { }
    T& get() const { return *m_ref; }
    operator T&() const { return *m_ref; }
    T* m_ref;
};
template<typename T, class... Args> UniqueRef<T> makeUniqueRef(Args&&... args)
{
    return UniqueRef<T>(*new T(std::forward<Args>(args)...));
}
using Mix = Notation<5, Params>;
int main() {
    Mix m { { 1.5, 2.5 } };
    UniqueRef<Mix> u(m);
    const UniqueRef<Mix>& cu = u;
    auto copy = makeUniqueRef<Mix>(cu);
    printf("%g %g %d\n", copy.get().parameters.a.v, copy.get().parameters.b.v, copy.m_ref != &m);
}
