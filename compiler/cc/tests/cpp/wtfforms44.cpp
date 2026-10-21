// A class's own operator new beside placement forms, used by new T and make_unique; moving a unique_ptr does not write its empty deleter's byte over the pointer
#include <stdio.h>
#include <stdlib.h>
#include <memory>

namespace p_cnew2 {
struct NotNullTag {};
static int news, dels;
struct M {
    void* operator new(size_t, void* p) { return p; }
    void* operator new(size_t size) { news++; return malloc(size); }
    void operator delete(void* p) { dels++; free(p); }
    void* operator new(size_t, NotNullTag, void* location) { return location; }
    int x;
};
struct M2 {
    void* operator new(size_t size) { news++; return malloc(size); }
    void* operator new(size_t, void* p) { return p; }
    void operator delete(void* p) { dels++; free(p); }
    int x;
};
template <class T> T* mk() { return new T(); }
int run() {
    M* a = new M; printf("%d\n", news);
    M* b = new M(); printf("%d\n", news);
    M* c = mk<M>(); printf("%d\n", news);
    M2* d = new M2; printf("%d\n", news);
    delete a; delete b; delete c; delete d;
    printf("%d %d\n", news, dels);
    return 0;
}
}

namespace p_cnew8 {
static int news, dels;
struct M {
    void* operator new(size_t size) { news++; return malloc(size); }
    void operator delete(void* p) { dels++; free(p); }
    int x;
};
int run() {
    { auto m = std::make_unique<M>(); std::unique_ptr<M> g; g = std::move(m); printf("a %d %d\n", news, dels); }
    printf("%d %d\n", news, dels);
    { auto m = std::make_unique<M>(); std::unique_ptr<M> g; g = std::move(m); g = nullptr; printf("b %d %d\n", news, dels); }
    return 0;
}
}

int main() {
    p_cnew2::run();
    p_cnew8::run();
    return 0;
}
