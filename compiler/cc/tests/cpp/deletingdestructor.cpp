// `delete p` through a pointer to a class with a virtual destructor frees
// the object as its dynamic class says: the deleting destructor (D0, in
// the vtable after the destructor) destroys the complete object and calls
// that class's operator delete (its own or a base's, with the size when
// it takes it), else the global one. WebCore's DummyStorageProvider,
// TZone-allocated, deleted as a StorageProvider, was freed by the global
// operator delete.
#include <cstdio>
#include <cstdlib>
#include <new>

struct Base {
    virtual ~Base() { std::printf("~Base\n"); }
};
struct Own final : Base {
    ~Own() override { std::printf("~Own\n"); }
    static void* operator new(std::size_t size) { std::printf("Own new\n"); return std::malloc(size); }
    static void operator delete(void* p) { std::printf("Own delete\n"); std::free(p); }
};
struct Sized : Base {
    long payload[3] { };
    static void* operator new(std::size_t size) { return std::malloc(size); }
    static void operator delete(void* p, std::size_t size) { std::printf("Sized delete %d\n", (int)size); std::free(p); }
};
struct FastAllocated {
    static void* operator new(std::size_t size) { return std::malloc(size); }
    static void operator delete(void* p) { std::printf("FastAllocated delete\n"); std::free(p); }
};
struct Mixed : FastAllocated, Base {
    ~Mixed() override { std::printf("~Mixed\n"); }
};
struct Other {
    virtual ~Other() { std::printf("~Other\n"); }
    int other = 5;
};
struct Second : Base, Other {
    ~Second() override { std::printf("~Second\n"); }
    static void operator delete(void* p) { std::printf("Second delete\n"); std::free(p); }
    static void* operator new(std::size_t size) { return std::malloc(size); }
};
struct Plain : Base { };
struct Virtual : virtual Base {
    ~Virtual() override { std::printf("~Virtual\n"); }
    static void operator delete(void* p) { std::printf("Virtual delete\n"); std::free(p); }
    static void* operator new(std::size_t size) { return std::malloc(size); }
};

int main()
{
    Base* b = new Own;
    delete b;
    b = new Sized;
    delete b;
    b = new Mixed;
    delete b;
    Other* o = new Second;
    delete o;
    b = new Plain;
    delete b;
    b = new Virtual;
    delete b;
    Own* own = new Own;
    delete own;
    return 0;
}
