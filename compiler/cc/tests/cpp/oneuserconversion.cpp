// One user-defined conversion at most for an argument: a Ref<T> to
// WeakHashSet::add(WeakRef<T> &&) would need Ref's operator T & and then
// WeakRef(const T &), so add(const T &) is taken (WTF's, in WebCore's
// EventLoop: the other was chosen, the WeakRef made through its explicit
// constructor from a Ref<Impl> that was the Ref<T> reinterpreted).
#include <stdio.h>
template<typename T> struct Ref {
    T *p;
    Ref(T &o) : p(&o) {}
    template<typename X> Ref(const Ref<X> &other) : p(&other.get()) {}
    T &get() const { return *p; }
    operator T &() const { return *p; }
};
struct Impl { int i = 9; };
struct Timer { int t = 5; };
template<typename T> struct WeakRef {
    explicit WeakRef(Ref<Impl> &&impl) : v(impl.get().i) {}
    WeakRef(const T &object) : v(object.t * 10) {}
    int v;
};
template<typename T> struct Set {
    int add(const T *value) { return value->t; }
    int add(const T &value) { return add(&value) + 100; }
    int add(WeakRef<T> &&value) { return value.v + 1000; }
};

int main(int argc, char **argv)
{
    Timer timer;
    Ref<Timer> r(timer);
    Set<Timer> s;
    printf("%d\n", s.add(r));
    return argc;
}
