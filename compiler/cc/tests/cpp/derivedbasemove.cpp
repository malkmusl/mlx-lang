// An aggregate base class constructed from an rvalue of a class derived
// from it (`Info(std::move(request))`, the placement new of a
// Vector<Info>'s append(Request&&)) moves the base part: its members'
// move constructors, the source's handles emptied. Copied, both objects
// kept the handle and the request's destruction freed it (WebCore's Web
// Locks registry, LockRequest moved into a Vector<LockInfo>).
#include <stdio.h>
#include <new>
#include <utility>

static int live = 0;
struct Handle {
    int* p = nullptr;
    Handle() = default;
    explicit Handle(int v) : p(new int(v)) { live++; }
    Handle(Handle&& o) : p(o.p) { o.p = nullptr; }
    Handle& operator=(Handle&& o) { reset(); p = o.p; o.p = nullptr; return *this; }
    ~Handle() { reset(); }
    void reset() { if (p) { delete p; live--; p = nullptr; } }
};
struct Info {
    int id;
    Handle handle;
};
struct Request : Info {
    int extra;
    Handle granted;
};

template<typename T> struct Box {
    alignas(T) unsigned char storage[sizeof(T)];
    template<typename U> void put(U&& value) { new (storage) T(std::forward<U>(value)); }
    T& get() { return *reinterpret_cast<T*>(storage); }
};

int main()
{
    {
        Request request { { 7, Handle(1) }, 3, Handle(2) };
        Box<Info> box;
        box.put(std::move(request));
        printf("box %d %d, source %d, granted %d\n", box.get().id, *box.get().handle.p, request.handle.p != nullptr, *request.granted.p);
        box.get().~Info();
    }
    {
        Request request { { 8, Handle(4) }, 5, Handle(6) };
        Info direct(std::move(request));
        printf("direct %d %d, source %d\n", direct.id, *direct.handle.p, request.handle.p != nullptr);
    }
    {
        Request request { { 9, Handle(7) }, 5, Handle(8) };
        Info* heap = new Info(std::move(request));
        printf("heap %d %d, source %d\n", heap->id, *heap->handle.p, request.handle.p != nullptr);
        delete heap;
    }
    printf("live %d\n", live);
    return 0;
}
