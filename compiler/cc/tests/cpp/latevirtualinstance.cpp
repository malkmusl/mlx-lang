// A class template's instance first made at the end of the unit (by a
// body read there: a function template defined after its call), its base's
// virtual member then needing a body that uses the instance itself
// (WebCore's GenericCachedHTMLCollection<...> made in Ref's destructor,
// CachedHTMLCollection's length() calling the collection's
// willValidateIndexCache()): that body waits for the instance to be
// complete.
#include <stdio.h>

template<class Collection> struct IndexCache {
    unsigned nodeCount(const Collection &c);
    unsigned n = 0;
};

template<class Collection> unsigned IndexCache<Collection>::nodeCount(const Collection &c)
{
    c.willValidate();
    return c.count + 1;
}

struct Base {
    virtual ~Base() { }
    virtual unsigned length() const = 0;
};

template<class C> struct Cached : Base {
    unsigned length() const override;
    const C &collection() const { return static_cast<const C &>(*this); }
    mutable IndexCache<C> cache;
};

template<class C> unsigned Cached<C>::length() const
{
    return cache.nodeCount(collection());
}

template<int N> struct Generic;

template<class T> struct Ref {
    T *p;
    ~Ref();
};

template<int N> Base *make();

int run(int argc)
{
    Ref<Generic<17>> r { (Generic<17> *)make<17>() };
    return argc;
}

template<class T> Ref<T>::~Ref()
{
    printf("length %u\n", p->length());
    delete p;
}

template<int N> struct Generic : Cached<Generic<N>> {
    int count = N;
    void willValidate() const { printf("validate %d\n", N); }
};

template<int N> Base *make() { return new Generic<N>; }

int main(int argc, char **)
{
    return run(argc);
}
