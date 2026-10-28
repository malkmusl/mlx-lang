// `auto x = c ? T(a) : T(b)`, T holding a vector with an inline buffer:
// the branch's temporary made at the result by its constructor, not
// moved there by its bytes, which left the result's buffer pointing into
// the dead temporary (the DFG's `auto callLinkStatus = f ?
// CallLinkStatus(CallVariant(f)) : CallLinkStatus(...)`, freed as heap);
// nor made there again from its arguments, whose temporaries the branch
// not taken never made (LinkBuffer's `CodeRef c(m ? CodeRef(*m) :
// CodeRef::createSelfManagedCodeRef(code))`).
#include <stdio.h>
struct Vec {
    int* buffer;
    int size;
    int inlineStorage[1];
    Vec(int v) : buffer(inlineStorage), size(1) { inlineStorage[0] = v; }
    Vec(const Vec& o) : buffer(inlineStorage), size(o.size) { inlineStorage[0] = o.buffer[0]; }
    Vec(Vec&& o) : buffer(inlineStorage), size(o.size) { inlineStorage[0] = o.buffer[0]; }
    ~Vec() { if (buffer != inlineStorage) printf("a buffer not its own\n"); }
};
struct Status { explicit Status(int v) : list(v) { } Vec list; bool flag = true; };
__attribute__((noinline)) Status make(int v) { return Status(v * 10); }
__attribute__((noinline)) int use(const Status& s) { return s.list.buffer[0]; }
struct Handle { int count = 1; };
struct Ref { Handle* p; explicit Ref(Handle& h) : p(&h) { h.count++; } Ref(Ref&& o) : p(o.p) { o.p = nullptr; } ~Ref() { if (p) p->count--; } };
struct Code {
    Ref* ref;
    int self;
    explicit Code(Handle& h) : ref(new Ref(Ref(h))), self(0) { }
    Code(int s) : ref(nullptr), self(s) { }
    Code(Code&& o) : ref(o.ref), self(o.self) { o.ref = nullptr; }
    ~Code() { delete ref; }
    static Code selfManaged(int s) { return Code(s); }
};
int main(int argc, char**)
{
    Handle handle;
    for (int i = 0; i < 2; ++i) {
        Handle* memory = i ? &handle : nullptr;
        Code code(memory ? Code(*memory) : Code::selfManaged(7));
        printf("%d %d %d\n", code.ref != nullptr, code.self, handle.count);
    }
    printf("%d\n", handle.count);
    for (int i = 0; i < 2; ++i) {
        auto a = i ? Status(1) : Status(2);
        auto b = i ? Status(3) : make(4);
        int c = use(i ? Status(5) : Status(6));
        printf("%d %d %d %d\n", a.list.buffer == a.list.inlineStorage, a.list.buffer[0], b.list.buffer[0], c);
    }
    return 0;
}
