// A union whose members include classes with constructors (trivially
// copyable still) is copied, constructed and assigned, as its bytes,
// whichever member is in use: made member by member, the bytes no
// member's fields cover (the padding of each) were left behind (bison's
// YYSTYPE in ANGLE's GLSL parser, shaped as here: a TPublicType's array
// sizes pointer lost bytes, `float[3](...)` crashed).
#include <stdio.h>
#include <string.h>
#include <array>
struct Loc { int a, b, c, d; };
struct Spec { int type; unsigned char p, s; const void *user; Loc line; bool st, sv; };
class WorkGroupSize { public: WorkGroupSize() = default; explicit constexpr WorkGroupSize(int v) : sizes{v, v, v} {} std::array<int, 3> sizes; };
class Blend { public: Blend() = default; explicit constexpr Blend(unsigned v) : bits(v) {} bool any() const { return bits != 0; } private: unsigned bits; };
struct Layout {
    Layout() = default;
    constexpr static Layout create() { return Layout(0); }
    int location; unsigned specified; int packing; int storage; WorkGroupSize local; int binding; int offset; bool push; int depth; int format; int views; bool yuv; bool early; int prim; int inv; int maxv; int verts; int t1, t2, t3, t4; int index; int input; bool noncoherent; Blend blend; bool ordered;
  private:
    constexpr explicit Layout(int) : location(-1), specified(0), packing(0), storage(0), local(-1), binding(-1), offset(-1), push(false), depth(0), format(0), views(-1), yuv(false), early(false), prim(0), inv(0), maxv(-1), verts(0), t1(0), t2(0), t3(0), t4(0), index(-1), input(-1), noncoherent(false), blend(0), ordered(false) {}
};
struct Memory { Memory() = default; constexpr static Memory create() { return Memory(0); } bool r, w, c, re, v; private: constexpr explicit Memory(int) : r(false), w(false), c(false), re(false), v(false) {} };
struct Public { Spec spec; Layout layout; Memory memory; int qualifier; bool invariant; bool precise; int precision; const void *array; };
struct Pair { void *a, *b; };
union Value {
    struct { union { const char *string; float f; int i; }; const void *symbol; } lex;
    struct { int op; union { void *node; Pair pair; }; union { void *sizes; Spec spec; Public type; Layout layout; int q; }; } interm;
};
int main() {
    static Value stack[3];
    Value value;
    memset(&value, 0xAB, sizeof value);
    Value *at = &stack[0];
    *++at = value;
    const unsigned char *b = (const unsigned char *)at;
    int n = 0; for (size_t i = 0; i < sizeof(Value); i++) n += b[i] == 0xAB;
    Value copied(value);
    b = (const unsigned char *)&copied;
    int m = 0; for (size_t i = 0; i < sizeof(Value); i++) m += b[i] == 0xAB;
    printf("%zu %d %d\n", sizeof(Value), n, m);
    return 0;
}
