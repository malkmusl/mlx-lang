// A derived object passed for a base by value: the base's part, copied by the base's copy constructor (WTF's ScopedLambdaFunctor passed as a ScopedLambda keeps pointing at the original).
#include <stdio.h>
#include <optional>
#include <utility>

namespace p_scl4 {
struct Base { int id; void* self; Base(int i, void* s) : id(i), self(s) {} };
struct Derived : Base {
    int extra;
    Derived(int i) : Base(i, this), extra(99) {}
    Derived(const Derived& o) : Base(o.id + 100, this), extra(o.extra) { printf("Derived copy\n"); }
};
void take(Base b) { printf("%d %d\n", b.id, b.self != 0); }
void takeRef(const Base& b) { printf("%d\n", b.id); }
int run() {
    Derived d(1);
    take(d);
    takeRef(d);
    Base b2 = d; printf("%d %d\n", b2.id, b2.self == (void*)&d);
    return 0;
}
}

namespace p_scl {
template<typename F> class SL;
template<typename R, typename... A> class SL<R(A...)> {
public:
    SL(R (*impl)(void*, A...) = nullptr, void* arg = nullptr) : m_impl(impl), m_arg(arg) {}
    template<typename... P> R operator()(P&&... p) const { return m_impl(m_arg, std::forward<P>(p)...); }
private:
    R (*m_impl)(void*, A...);
    void* m_arg;
};
template<typename F, typename Fn> class SLF;
template<typename R, typename... A, typename Fn> class SLF<R(A...), Fn> : public SL<R(A...)> {
public:
    template<typename P> SLF(P&& f) : SL<R(A...)>(implFunction, this), m_functor(std::forward<P>(f)) {}
    SLF(const SLF& o) : SL<R(A...)>(implFunction, this), m_functor(o.m_functor) {}
private:
    static R implFunction(void* arg, A... a) { return static_cast<SLF*>(arg)->m_functor(a...); }
    Fn m_functor;
};
template<typename F, typename Fn> SLF<F, Fn> scopedLambda(const Fn& f) { return SLF<F, Fn>(f); }
struct Lock { void lock() {} void unlock() {} };
struct Solver {
    Lock m_lock; bool active = true; int total = 0;
    template <typename Fn> void runInParallel(const Fn& f) { f(7); }
    void runThread(int v, int pref, SL<std::optional<unsigned>()> pickNext) {
        for (;;) {
            int constraint = -1;
            {
                for (;;) {
                    auto tryNext = [&] () -> bool {
                        if (!active) return false;
                        for (;;) {
                            std::optional<unsigned> r = pickNext();
                            if (!r) { active = false; return false; }
                            constraint = (int)*r;
                            return true;
                        }
                    };
                    if (tryNext()) break;
                    return;
                }
            }
            total += constraint * v + pref;
        }
    }
    void execute(int pref, SL<std::optional<unsigned>()> pickNext) {
        active = true;
        runInParallel([&] (int v) { runThread(v, pref, pickNext); });
    }
    void drain(unsigned n) {
        unsigned i = 0;
        auto pickNext = scopedLambda<std::optional<unsigned>()>([&] () -> std::optional<unsigned> { if (i == n) return std::nullopt; return i++; });
        execute(1, pickNext);
    }
};
int run() { Solver s; s.drain(5); printf("%d\n", s.total); return 0; }
}

int main() {
    p_scl4::run();
    p_scl::run();
    return 0;
}
