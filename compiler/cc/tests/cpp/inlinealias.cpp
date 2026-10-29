// Inlined bodies over wrappers of one scalar: a member function returning
// a reference (operator+=, an accessor's int&), `this` and reference
// parameters bound as aliases of the locals they name (through `&*p`, a
// compound assignment's temporary, a pointer passed on to another
// inlined member), a call that is a statement, a struct result written
// into its buffer, a copy made of a parameter's bytes; gcc prints the
// same. The LIR promotes such locals' places to slots.
#include <stdio.h>
#include <stddef.h>
#include <cstddef>

struct LU {
    int v;
    LU(int x = 0) : v(x) {}
    LU& operator+=(LU o) { v += o.v; return *this; }
    LU& addTo(LU o) { v += o.v; return *this; }
    LU plus(LU o) const { return LU(v + o.v); }
    void addIn(LU o) { v += o.v; }
    int& ref() { return v; }
};
inline LU& addFree(LU& a, LU b) { a.v += b.v; return a; }
int total(const LU* p, int n) { LU sum; for (int i = 0; i < n; i++) sum += p[i]; return sum.v; }
int total2(const LU* p, int n) { LU sum; for (int i = 0; i < n; i++) sum.addTo(p[i]); return sum.v; }
int total3(const LU* p, int n) { LU sum; for (int i = 0; i < n; i++) sum = sum.plus(p[i]); return sum.v; }
int total4(const LU* p, int n) { LU sum; for (int i = 0; i < n; i++) sum.addIn(p[i]); return sum.v; }
int total5(const LU* p, int n) { LU sum; for (int i = 0; i < n; i++) addFree(sum, p[i]); return sum.v; }
int total6(const LU* p, int n) { LU sum; for (int i = 0; i < n; i++) sum.ref() += p[i].v; return sum.v; }
int total7(const LU* p, int n) { LU sum; for (int i = 0; i < n; i++) sum.operator+=(p[i]); return sum.v; }
int chained(const LU* p) { LU sum; (sum += p[0]) += p[1]; return (sum.addTo(p[2]).ref() += 100); }

enum class Tag { A, B };
template<Tag tag, int kind = 0> class CP {
public:
    CP() = default;
    static CP fromRaw(char* p) { CP r; r.m = p; return r; }
    template<typename T = void*> T raw() const { return (T)m; }
    CP operator+(size_t n) const { return CP::fromRaw(raw<char*>() + n); }
    CP& operator+=(size_t n) { return *this = *this + n; }
    char* m = nullptr;
};

struct Ord {
    signed char v;
    constexpr Ord(signed char x) : v(x) {}
    static const Ord less, equal, greater;
    friend bool operator<(Ord a, int) { return a.v < 0; }
    friend bool operator>(Ord a, int) { return a.v > 0; }
    friend bool operator==(Ord a, int) { return a.v == 0; }
};
const Ord Ord::less(-1);
const Ord Ord::equal(0);
const Ord Ord::greater(1);
static Ord cmp(int a, int b) { return a < b ? Ord::less : a > b ? Ord::greater : Ord::equal; }
const char *name(Ord o) { return o < 0 ? "less" : o > 0 ? "greater" : "equal"; }

struct Impl { int v; };
template<typename T> struct AlignedPtr { T* m; constexpr AlignedPtr() : m(nullptr) {} constexpr AlignedPtr(std::nullptr_t) : m(nullptr) {} AlignedPtr(T* value) : m(value) {} T* get() const { return m; } };
struct Deleted {};
template<typename T> struct Packed : public AlignedPtr<T> { using Base = AlignedPtr<T>; using Base::Base; Packed(Deleted) : Base((T*)1) {} };
template<typename T> struct Set { int n = 0; int add(const T& e) { n += 1; return e.get()->v; } int add(T&& e) { n += 10; return e.get()->v; } };
int take(Packed<Impl> p) { return p.get()->v; }

struct Point { float x, y; Point(float a, float b) : x(a), y(b) {} Point& operator+=(const Point& o) { x += o.x; y += o.y; return *this; } float norm1() const { return (x < 0 ? -x : x) + (y < 0 ? -y : y); } };
float walk(const Point* steps, int n) { Point at(0.5f, -0.25f); for (int i = 0; i < n; i++) at += steps[i]; return at.norm1(); }

int main(void) {
    LU a[5];
    for (int i = 0; i < 5; i++) a[i] = LU(i * 3 + 1);
    printf("%d %d %d %d %d %d %d %d\n", total(a, 5), total2(a, 5), total3(a, 5), total4(a, 5), total5(a, 5), total6(a, 5), total7(a, 5), chained(a));
    char buf[8];
    auto p = CP<Tag::A>::fromRaw(buf);
    p += 3;
    printf("%d\n", (int)(p.m - buf));
    printf("%s %s %s\n", name(cmp(1, 2)), name(cmp(3, 2)), name(cmp(2, 2)));
    Ord o = cmp(5, 9);
    printf("%d %d %d\n", o < 0, o > 0, o == 0);
    Set<Packed<Impl>> s;
    Impl i{3};
    int r = s.add(&i);
    Packed<Impl> q = &i;
    printf("%d %d %d %d\n", s.n, r, take(&i), q.get()->v);
    Point steps[3] = { Point(1, 2), Point(-3, 0.5f), Point(0.25f, 0.25f) };
    printf("%g\n", walk(steps, 3));
    return 0;
}
