// `auto f(T t)` returning a T: a class passed by its address and the
// result buffer of a deduced result are both pointers to T as hidden
// first parameters; the result's buffer is still added (WTF's byteCast
// of a std::span to a span of the same bytes read its data from a
// parameter's address).
#include <cstdio>
#include <cstddef>
template<typename T> struct S { T* p; size_t n; S(T* a, size_t b) : p(a), n(b) { } S(const S& o) : p(o.p), n(o.n) { } };
struct C { unsigned char* p; size_t n; C(unsigned char* a, size_t b) : p(a), n(b) { } C(const C& o) : p(o.p), n(o.n) { } };
static auto h1(unsigned char* p) { return S<unsigned char>(p, 5); }
static S<unsigned char> h2(unsigned char* p) { return S<unsigned char>(p, 5); }
static auto h3(unsigned char* p) { return C(p, 5); }
static auto h4(S<unsigned char> s) { return S<unsigned char>(s.p, s.n); }
int main()
{
    unsigned char out[5];
    auto a = h1(out); std::printf("h1 %d %zu\n", a.p == out, a.n);
    auto b = h2(out); std::printf("h2 %d %zu\n", b.p == out, b.n);
    auto c = h3(out); std::printf("h3 %d %zu\n", c.p == out, c.n);
    auto d = h4(S<unsigned char>(out, 5)); std::printf("h4 %d %zu\n", d.p == out, d.n);
    return 0;
}
