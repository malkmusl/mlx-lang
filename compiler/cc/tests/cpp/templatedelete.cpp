// `delete p` of a class template's instance not made yet where the
// delete is read (known only through pointers so far): the instance is
// made there, so its own operator delete (WTF's fast-allocated classes)
// and its virtual destructor are the ones used.
#include <cstdio>
#include <cstdlib>
struct Allocated {
    void* operator new(size_t size) { printf("class new\n"); return malloc(size); }
    void operator delete(void* p) { printf("class delete\n"); free(p); }
    virtual ~Allocated() { }
};
template<typename Out> struct WrapperBase : Allocated { virtual ~WrapperBase() { printf("~WrapperBase\n"); } virtual Out call() = 0; };
template<typename T> struct Deleter { void operator()(T* p) const { delete p; } };
void destroy(WrapperBase<int>* p) { Deleter<WrapperBase<int>>()(p); }
template<typename Out> struct Wrapper : WrapperBase<Out> { ~Wrapper() { printf("~Wrapper\n"); } Out call() override { return 7; } };
int main() {
    WrapperBase<int>* p = new Wrapper<int>;
    printf("call %d\n", p->call());
    destroy(p);
    printf("end\n");
    return 0;
}
