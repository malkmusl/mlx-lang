// Rvalue references: T && binds rvalues only and before const T &;
// move constructors and move assignments chosen for temporaries, call
// results and objects named through std::move; forwarding references
// deduced from lvalues as references (collapsing); remove_reference by
// partial specialization with a dependent member type in a signature;
// decltype of calls and members; declval declared only, called where
// nothing is evaluated.
// (The expected output is g++'s: makeBuffer's `b`, a class returned in
// memory, is made in the caller's object (the named return value), and
// no other copy is elided.)
#include <cstdio>
namespace my {
template <class T> struct remove_reference { typedef T type; };
template <class T> struct remove_reference<T &> { typedef T type; };
template <class T> struct remove_reference<T &&> { typedef T type; };
template <class T> typename remove_reference<T>::type &&move(T &&x) { return static_cast<typename remove_reference<T>::type &&>(x); }
template <class T> T &&forward(typename remove_reference<T>::type &x) { return static_cast<T &&>(x); }
template <class T> T &&declval();
}
struct Buffer {
    int *data;
    int size;
    Buffer(int n) : data(new int[n]), size(n) { for (int i = 0; i < n; i++) data[i] = i; printf("make %d\n", n); }
    Buffer(const Buffer &other) : data(new int[other.size]), size(other.size) { for (int i = 0; i < size; i++) data[i] = other.data[i]; printf("copy %d\n", size); }
    Buffer(Buffer &&other) : data(other.data), size(other.size) { other.data = 0; other.size = 0; printf("move %d\n", size); }
    Buffer &operator=(const Buffer &other) { printf("copy= %d\n", other.size); delete[] data; data = new int[other.size]; size = other.size; for (int i = 0; i < size; i++) data[i] = other.data[i]; return *this; }
    Buffer &operator=(Buffer &&other) { printf("move= %d\n", other.size); delete[] data; data = other.data; size = other.size; other.data = 0; other.size = 0; return *this; }
    ~Buffer() { delete[] data; }
};
const char *kind(int &) { return "lvalue"; }
const char *kind(int &&) { return "rvalue"; }
const char *kind(const int &) { return "const lvalue"; }
template <class T> const char *passed(T &&x) { return kind(my::forward<T>(x)); }
Buffer makeBuffer(int n) { Buffer b(n); return b; }
int twice(int x) { return x + x; }
int main() {
    int a = 1;
    const int c = 2;
    printf("%s %s %s %s\n", kind(a), kind(3), kind(c), kind(my::move(a)));
    printf("%s %s %s\n", passed(a), passed(4), passed(c));
    Buffer x(3);
    Buffer y(x);
    Buffer z(my::move(x));
    printf("%d %d %d\n", x.size, y.size, z.size);
    Buffer w(makeBuffer(2));
    y = my::move(w);
    z = y;
    printf("%d %d %d\n", w.size, y.size, z.size);
    decltype(twice(1)) t = 5;
    decltype(my::declval<Buffer>().size) s = 6;
    printf("%d %d %lu\n", t, s, (unsigned long)sizeof(my::declval<Buffer &>().data));
    return 0;
}
