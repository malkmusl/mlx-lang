// A constrained member template defined outside its class template,
// beside an unconstrained one of the name declared first (WebCore's
// JSDOMAsyncIteratorPrototype::addReturnMethodIfNeeded): the definition
// is the constrained one's.
#include <cstdio>

template<typename T> concept HasReturn = requires(T& t) { t.returnSteps(); };

struct Plain { };
struct WithReturn { void returnSteps() { } };

template<typename Wrapper, typename Traits> class Prototype {
public:
    using Iterator = typename Wrapper::Iterator;
    void finishCreation()
    {
        printf("finish\n");
        addReturnMethodIfNeeded<Iterator>(1);
    }

private:
    template<typename I> void addReturnMethodIfNeeded(int) { printf("no return method\n"); }
    template<HasReturn I> void addReturnMethodIfNeeded(int);
};

template<typename Wrapper, typename Traits>
template<HasReturn I>
void Prototype<Wrapper, Traits>::addReturnMethodIfNeeded(int)
{
    printf("return method\n");
}

struct A { using Iterator = Plain; };
struct B { using Iterator = WithReturn; };

int main()
{
    Prototype<A, int> a;
    a.finishCreation();
    Prototype<B, int> b;
    b.finishCreation();
    return 0;
}
