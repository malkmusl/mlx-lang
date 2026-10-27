// `new (n) T` with one argument that is no address (Skia's GrProcessor
// classes: `new (uniformSize) GrSkSLFP(...)` takes a footer's size): the
// class's operator new taking it, not the argument used as the address.
#include <cstdio>
#include <cstdlib>
#include <new>

struct Base {
    void* operator new(size_t size) { return malloc(size); }
    void* operator new(size_t size, size_t footer)
    {
        std::printf("footer %zu\n", footer);
        return malloc(size + footer);
    }
    void* operator new(size_t size, void* placement) { return ::operator new(size, placement); }
    void operator delete(void* p) { free(p); }
    void operator delete(void*, void*) { }
    virtual ~Base() { }
    int id = 7;
};
struct Derived : Base {
    int more = 9;
};
static size_t footerSize(int n) { return n * 4; }

int main(int argc, char**)
{
    Base* a = new (footerSize(argc)) Derived;
    Base* b = new (footerSize(0)) Derived;
    alignas(Derived) unsigned char storage[sizeof(Derived)];
    Base* c = new (static_cast<void*>(storage)) Derived;
    std::printf("%d %d %d\n", a->id, b->id, c->id);
    delete a;
    delete b;
    c->~Base();
    return 0;
}
