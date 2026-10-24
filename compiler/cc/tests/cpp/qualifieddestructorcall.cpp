// A destructor called by its class's qualified name, the class a
// template's instance (`thisObject->JSDOMIteratorBase<JSWrapper,
// IteratorTraits>::~JSDOMIteratorBase()` in WebCore's JSDOMIterator.h:
// "expected an identifier"), and a plain class's, through `.` and `->`.
#include <stdio.h>
#include <new>

template<typename A, typename B> struct Base {
    int v;
    ~Base() { printf("~Base %d\n", v); }
    static void destroy(void *cell)
    {
        auto *self = static_cast<Base<A, B> *>(cell);
        self->Base<A, B>::~Base();
    }
};
struct Plain { ~Plain() { printf("~Plain\n"); } };

int main(int argc, char **argv)
{
    alignas(Base<int, char>) unsigned char storage[sizeof(Base<int, char>)];
    auto *b = new (storage) Base<int, char> { 4 };
    Base<int, char>::destroy(b);
    auto *c = new (storage) Base<int, char> { 5 };
    (*c).Base<int, char>::~Base();
    Plain p;
    p.Plain::~Plain();
    new (&p) Plain;
    return argc;
}
