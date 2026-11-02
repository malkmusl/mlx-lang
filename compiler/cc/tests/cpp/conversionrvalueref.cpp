// A `T &&` parameter and an object of another class converted by a
// conversion function: what the function returns initializes the
// reference directly ([dcl.init.ref] 5.4.1). One returning `T &` or
// `const T &` is an lvalue, which binds no `T &&`: the `const T &`
// overload is the one called. WebCore's `Attribute(nameAttr, value)`
// (nameAttr a LazyNeverDestroyed<const QualifiedName>) took
// `Attribute(QualifiedName &&, AtomString &&)` and moved the process's
// HTMLNames::nameAttr away in every text document.
#include <stdio.h>
#include <utility>
#include <new>

struct Name {
    int* impl;
    Name(int* p) : impl(p) {}
    Name(const Name& o) : impl(o.impl) {}
    Name(Name&& o) : impl(o.impl) { o.impl = nullptr; }
};
template<typename T> struct Lazy {
    alignas(T) unsigned char storage[sizeof(T)];
    template<typename... A> void construct(A&&... a) { new (storage) T(std::forward<A>(a)...); }
    operator T&() { return *reinterpret_cast<T*>(storage); }
    operator const T&() const { return *reinterpret_cast<const T*>(storage); }
};
struct ByValue { int* p; operator Name() const { return Name(p); } };
struct ByRvalue { Name n; operator Name&&() { return std::move(n); } };
struct ByLvalue { Name n; operator Name&() { return n; } };

static const char* take(const Name&) { return "const&"; }
static const char* take(Name&&) { return "&&"; }

struct Attr {
    Name name;
    const char* how;
    Attr(const Name& n, int) : name(n), how("copied") {}
    Attr(Name&& n, int) : name(std::move(n)), how("moved") {}
};

Lazy<const Name> constGlobal;
Lazy<Name> global;

int main()
{
    static int x = 5;
    constGlobal.construct(&x);
    global.construct(&x);
    Attr a(constGlobal, 1);
    const Name& kept = constGlobal;
    printf("const lazy: %s, %s, kept %d\n", take(constGlobal), a.how, kept.impl != nullptr);
    Attr b(global, 1);
    Name& still = global;
    printf("lazy: %s, %s, kept %d\n", take(global), b.how, still.impl != nullptr);
    ByValue v { &x };
    printf("by value: %s\n", take(v));
    ByRvalue r { Name(&x) };
    printf("by rvalue: %s\n", take(r));
    ByLvalue l { Name(&x) };
    Attr c(l, 1);
    printf("by lvalue: %s, %s, kept %d\n", take(l), c.how, l.n.impl != nullptr);
    return 0;
}
