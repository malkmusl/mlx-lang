// A using-declaration of a base's member type (and function) in a class
// with two bases that both have one: the base it names gives it
// (`using EventTarget::WeakPtrImplType;` in WebCore's Performance, its
// first base ContextDestructionObserver's another, for
// WeakRef<Performance, Performance::WeakPtrImplType>).
#include <stdio.h>

struct ImplA { int a = 1; };
struct ImplB { int b = 2; };

template<class Impl> struct Maker {
    using ImplType = Impl;
    Impl &weakImpl() const { return impl; }
    mutable Impl impl;
};

struct Observer : Maker<ImplA> { };
struct Target : Maker<ImplB> { };

struct Perf : Observer, Target {
    using Target::weakImpl;
    using Target::ImplType;
    int own() const { ImplType copy = weakImpl(); return copy.b * 10; }
};

template<class T, class Impl> struct WeakRef {
    WeakRef(const T &object) : impl(object.weakImpl()) { }
    Impl &impl;
};

int main(int argc, char **)
{
    Perf p;
    p.Target::impl.b = 5;
    WeakRef<Perf, Perf::ImplType> r(p);
    Perf::ImplType x;
    printf("%d %d %d\n", r.impl.b, x.b, p.own());
    return argc;
}
