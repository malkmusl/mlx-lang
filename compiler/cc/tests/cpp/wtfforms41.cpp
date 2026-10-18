// WTF/JSC forms: function templates by a pointer's signature, static locals assigned the first time
#include <stdio.h>
#include <string>

namespace p_tp2 {
struct Cell {};
struct ASV { int k = 0; };
struct SV : ASV {};
struct Table { void (*a)(Cell*, SV&); void (*b)(Cell*, ASV&); };
template<typename B> struct Impl {
    template<typename Visitor> static void visit(Cell*, Visitor&);
};
template<typename B> template<typename Visitor> void Impl<B>::visit(Cell*, Visitor& v) { v.k += sizeof(Visitor) + 100 * sizeof(B); }
struct Info { const char* name; Table t; };
static const Info s_info = { "W", { &Impl<long>::visit, &Impl<long>::visit } };
int run() { Cell c; SV s; ASV a; s_info.t.a(&c, s); s_info.t.b(&c, a); void (*p)(Cell*, ASV&) = &Impl<int>::visit; p(&c, a); printf("%d %d\n", s.k, a.k); return 0; }
}

namespace p_sl8 {
struct List { const char* names[2] = { "a", "fallback" }; const char* operator[](const std::string& n) const { for (auto x : names) if (n == x) return x; return nullptr; } };
static int calls = 0;
const List& get() { calls++; static const List l; return l; }
const char* builtin() { static const char* b = get()["fallback"]; return b; }
int run() { printf("%d ", calls); printf("%s %s %d\n", builtin(), builtin(), calls); return 0; }
}

int main() {
    p_tp2::run();
    p_sl8::run();
    return 0;
}
