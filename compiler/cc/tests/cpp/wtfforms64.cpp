// Objects that point into themselves (an inline buffer, as WTF's Vector<T, N>)
// returned and passed by value: a local returned is made in the caller's
// buffer when every return returns it, else moved out; a class that cannot
// move by its bytes is passed by its address (the Itanium ABI's invisible
// reference), never relocated byte by byte.
#include <stdio.h>
#include <string.h>
struct SP {
    char inl[16]; char* p; int n;
    SP() : p(inl), n(0) {}
    SP(const char* s) : p(inl), n((int)strlen(s)) { memcpy(inl, s, n + 1); }
    SP(const SP& o) : p(inl), n(o.n) { memcpy(inl, o.inl, 16); }
    SP(SP&& o) : p(inl), n(o.n) { memcpy(inl, o.inl, 16); o.n = 0; }
    ~SP() { if (p != inl) printf("BAD self pointer\n"); }
    bool ok() const { return p == inl; }
};
SP local(const char* s) { SP r(s); return r; }
SP local2(bool b) { SP a("a"), c("c"); if (b) return a; return c; }
SP temp() { return SP("t"); }
int byValue(SP s) { return s.ok() * 10 + s.n; }
int main() {
    SP x = local("hi"); printf("%d\n", x.ok());
    SP y = local2(true); SP z = local2(false); printf("%d %d %c %c\n", y.ok(), z.ok(), y.p[0], z.p[0]);
    printf("%d\n", byValue(SP("abc")));
    printf("%d\n", byValue(temp()));
    printf("%d\n", byValue(local("xyz")));
    SP w = SP("q"); printf("%d\n", w.ok());
    return 0;
}
