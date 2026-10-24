// static_cast<T>(e) of an object of the class T makes a temporary T as
// `T t(e);` does: the object copied, an xvalue moved (WTF's
// Markable::value_or, `c ? value : static_cast<T>(std::forward<U>(x))`),
// not the object's bytes taken while it still owns what they point to.
#include <cstdio>
#include <utility>
struct T {
    int id;
    T(int i) : id(i) { std::printf("make %d\n", id); }
    T(const T& o) : id(o.id + 100) { std::printf("copy %d -> %d\n", o.id, id); }
    T(T&& o) : id(o.id + 10) { std::printf("move %d -> %d\n", o.id, id); o.id = -o.id; }
    ~T() { std::printf("free %d\n", id); }
};
T f1(T&& x) { return static_cast<T>(std::move(x)); }
T f2(bool c, const T& l, T&& x) { return c ? l : static_cast<T>(std::move(x)); }
T f3(const T& l) { return static_cast<T>(l); }
int main()
{
    { T a(1); T b = static_cast<T>(std::move(a)); std::printf("b %d a %d\n", b.id, a.id); }
    { T a(2); T b = f1(std::move(a)); std::printf("b %d a %d\n", b.id, a.id); }
    { T a(3); T l(4); T b = f2(false, l, std::move(a)); std::printf("b %d a %d\n", b.id, a.id); }
    { T a(5); T b = f3(a); std::printf("b %d a %d\n", b.id, a.id); }
    { T a(6); const T& r = static_cast<T>(a); std::printf("r %d a %d\n", r.id, a.id); }
    return 0;
}
