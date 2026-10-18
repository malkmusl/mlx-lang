// new T takes the size-only operator new a base brings by a using-declaration (ScopeNode's using ParserArenaRoot::operator new beside the first base's placement form)
#include <stdio.h>
#include <stdlib.h>
#include <memory>

namespace p_usingnew {
static int fastNews, fastDels, arenaNews;
struct Arena { char buf[256]; int used = 0; };
struct Freeable { void *operator new(size_t n, Arena &a) { arenaNews++; void *p = a.buf + a.used; a.used += (int)n; return p; } };
struct Node : Freeable { virtual ~Node() {} int line = 0; };
struct Root { void *operator new(size_t n) { fastNews++; return malloc(n); } void operator delete(void *p) { fastDels++; free(p); } virtual ~Root() {} Arena arena; };
struct Deletable { virtual ~Deletable() {} template <typename T> void *operator new(size_t, Arena &); };
struct Scope : Node, Root, Deletable { using Root::operator new; int x = 1; };
struct Program final : Scope { int y = 2; };
int run() {
    { auto p = std::make_unique<Program>(); p->y = 3; }
    Program *q = new Program; delete q;
    printf("%d %d %d\n", fastNews, fastDels, arenaNews);
    return 0;
}
}

int main() {
    p_usingnew::run();
    return 0;
}
