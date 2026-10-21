// Of function templates taking `T &` (or `T &&`) and `const T &`, a const
// lvalue takes `const T &`, the more specialized ([temp.deduct.partial]):
// WTF's getPtr(T &) and getPtr(const T &) of a `const Ref<T>`, whose
// helper for `const Ref<T>` would return the Ref's own address.
#include <stdio.h>

template<class T> int f(T &) { return 1; }
template<class T> int f(const T &) { return 2; }
template<class T> int g(T &&) { return 1; }
template<class T> int g(const T &) { return 2; }
template<class T> int h(T *) { return 1; }
template<class T> int h(const T *) { return 2; }

template<class T> struct Ref { T *p; T *get() const { return p; } };
template<class T> struct Helper { static const void *ptr(const T &t) { return &t; } };
template<class T> struct Helper<Ref<T>> { static const void *ptr(const Ref<T> &r) { return r.get(); } };
template<class T> const void *getPtr(T &p) { return Helper<T>::ptr(p); }
template<class T> const void *getPtr(const T &p) { return Helper<T>::ptr(p); }

struct S { };

int main(int argc, char **)
{
    S s;
    const S cs {};
    printf("%d %d %d\n", f(s), f(cs), f(S {}));
    printf("%d %d %d\n", g(s), g(cs), g(S {}));
    printf("%d %d\n", h(&s), h(&cs));
    int target = 5;
    const Ref<int> r { &target };
    Ref<int> m { &target };
    printf("%d %d\n", getPtr(r) == &target, getPtr(m) == &target);
    return argc;
}
