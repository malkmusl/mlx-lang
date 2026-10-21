// A function template whose parameters all have defaults takes an object
// of a class converting to its parameter's type: WTF's OrderedHashSet
// `template<SmartPtr V = ValueType> bool remove(std::add_const_t<typename
// GetPtrHelper<V>::UnderlyingType> *)` given a RefPtr<T> (WebCore's
// ResizeObserver). Its parameter names V where C++ deduces nothing; V
// stays the default. The exact match of another overload still wins.
#include <stdio.h>
#include <type_traits>

struct Obj { int v; };

template<class T> struct Ref {
    T *p;
    explicit Ref(T &t) : p(&t) { }
    T *ptr() const { return p; }
};

template<class T> struct RefPtr {
    T *p;
    RefPtr(T *t) : p(t) { }
    operator T *() const { return p; }
    explicit operator bool() const { return !!p; }
};

template<class T> struct Helper { using Underlying = T; };
template<class T> struct Helper<Ref<T>> { using Underlying = T; };

template<class ValueType> struct Set {
    ValueType items[2];
    int count { 0 };
    int exact { 0 };
    template<class V = ValueType> bool remove(std::add_const_t<typename Helper<V>::Underlying> *value)
    {
        for (int i = 0; i < count; ++i) {
            if (items[i].ptr() == value) {
                items[i] = items[--count];
                return true;
            }
        }
        return false;
    }
    bool remove(const ValueType &value)
    {
        ++exact;
        return remove(value.ptr());
    }
};

template<class V = Obj> int viaDefault(std::add_const_t<V> *p) { return p->v + 20; }

int main(int argc, char **)
{
    Obj a { 1 }, b { 2 };
    Set<Ref<Obj>> s { { Ref<Obj>(a), Ref<Obj>(b) }, 2 };
    RefPtr<Obj> r(&a);
    bool removed = s.remove(r);
    Ref<Obj> rb(b);
    bool removedExact = s.remove(rb);
    printf("%d %d %d %d %d\n", (int)removed, (int)removedExact, s.count, s.exact, viaDefault(r));
    return argc;
}
