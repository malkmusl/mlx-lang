// A friend of a member class of a class template's instance, the instance
// read where a member function template of another class needs it
// (JSC's HandleSet::visitStrongHandles over SentinelLinkedList's
// iterator): declared in the namespace around the templates, which
// argument-dependent lookup searches, for `!=` rewritten to `==`.
#include <cstdio>
namespace W {
template<typename T> class List {
public:
    template<typename N> class It {
    public:
        explicit It(N* n) : m(n) { }
        N& operator*() const { return *m; }
        It& operator++() { ++m; return *this; }
        friend bool operator==(It, It) = default;
    private:
        N* m;
    };
    using iterator = It<T>;
    iterator begin() { return iterator { a }; }
    iterator end() { return iterator { a + 3 }; }
    T a[3] = { 1, 2, 3 };
};
}
struct S {
    W::List<int> list;
    template<typename V> int sum(V v);
};
template<typename V> int S::sum(V v)
{
    int t = 0;
    for (int& x : list)
        t += x * v;
    return t;
}
template int S::sum(int);
int main()
{
    S s;
    std::printf("%d\n", s.sum(2));
    W::List<int> l;
    std::printf("%d\n", l.begin() != l.end());
    return 0;
}
