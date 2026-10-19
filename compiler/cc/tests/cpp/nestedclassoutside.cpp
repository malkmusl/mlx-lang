// A class template's member class defined outside it (`template<typename
// T, typename UserData> class PODIntervalTree<T, UserData>::
// OverlapsSearchAdapter { ... };` in WebCore's PODIntervalTree.h:
// "expected ';'"), for the instances made after and before it, naming
// the class's parameters and members.
#include <stdio.h>

template<typename T, typename U> class Tree {
public:
    using Value = T;
    class Adapter;
    int count(T v);
    U extra = 2;
};

Tree<short, char> early;

template<typename T, typename U> class Tree<T, U>::Adapter {
public:
    Adapter(Value v) : m_v(v) {}
    int get() const { return (int)m_v + (int)sizeof(U); }
    int with(const Tree &tree) const { return get() + tree.extra; }
private:
    T m_v;
};

template<typename T, typename U> int Tree<T, U>::count(T v)
{
    Adapter a(v);
    return a.with(*this);
}

int main(int argc, char **argv)
{
    Tree<int, char> t;
    Tree<long, int>::Adapter b(5);
    Tree<short, char>::Adapter c(1);
    printf("%d %d %d %d\n", t.count(3), b.get(), early.count(6), c.with(early));
    return argc;
}
