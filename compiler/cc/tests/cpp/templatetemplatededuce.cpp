// A member template's parameter `U<T>`, U a template template parameter
// and T the class's own: U is deduced from the argument's template
// (WebCore's JSConverter<IDLPromise<T>>::convert(..., U<T> &) taking a
// DOMPromiseProxy<T>), beside an overload taking a class by reference.
#include <cstdio>

template<typename T> struct ProxyA { int promise() { return 1; } };
template<typename T> struct ProxyB { int promise() { return 2; } };
struct Promise { int guarded() const { return 3; } };

template<typename T> struct Converter {
    static int convert(const Promise& promise) { return promise.guarded(); }
    template<template<typename> class U>
    static int convert(U<T>& proxy) { return proxy.promise() * 10; }
};

int main()
{
    ProxyA<int> a;
    ProxyB<int> b;
    Promise p;
    printf("%d %d %d\n", Converter<int>::convert(a), Converter<int>::convert(b), Converter<int>::convert(p));
    return 4;
}
