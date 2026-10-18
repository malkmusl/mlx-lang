// is_constructible / is_assignable of a class from another class that
// converts to it through a conversion operator (JSC's GCOwnedDataScope<const
// String&> assigned to a Variant<String, ...>: the converting assignment's
// enable_if asks both).
#include <type_traits>
#include <cstdio>
struct Str { Str() {} Str(const char*) {} Str(const Str&) {} Str& operator=(const Str&) { return *this; } };
struct View { View(const Str&) {} };
template<typename T> struct Scope {
    Scope(T v) : data(v) {}
    operator const T() const { return data; }
    operator T() { return data; }
    T data;
};
struct Plain { operator Str() const { return Str(); } };
int main() {
    printf("%d %d %d %d\n",
        (int)std::is_constructible<Str, Scope<const Str&>>::value,
        (int)std::is_assignable<Str&, Scope<const Str&>>::value,
        (int)std::is_constructible<Str, Plain>::value,
        (int)std::is_assignable<Str&, Plain>::value);
    printf("%d %d\n", (int)std::is_convertible<Scope<const Str&>, Str>::value, (int)std::is_convertible<Plain, Str>::value);
    return 0;
}
