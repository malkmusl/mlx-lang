// Overloads of a name in three namespaces, joined by argument-dependent
// lookups in some order: each namespace still finds its own.
#include <cstdio>

namespace C {
struct CT { int v; };
bool operator==(const CT& a, const CT& b) { return a.v == b.v; }
}

namespace W {
struct WT { int v; };
bool operator==(const WT& a, const C::CT& b) { return a.v == b.v; }
}

namespace A {
struct AT { int v; };
bool operator==(const AT& a, const C::CT& b) { return a.v == b.v; }
bool operator==(const AT& a, const W::WT& b) { return a.v == b.v; }
}

bool first(int x) { return W::WT { x } == C::CT { 1 }; }

namespace W {
struct BP { int offset; };
bool operator==(const BP& a, const BP& b) { return a.offset == b.offset; }
}

bool second(int x) { return A::AT { x } == C::CT { 2 }; }
bool third(int x) { return A::AT { x } == W::WT { 3 }; }

namespace W {
bool same(const BP& a, const BP& b) { return a == b; }
}

int main(int argc, char** argv)
{
    W::BP p { 4 };
    W::BP q { argc };
    std::printf("%d %d %d\n", (int)first(argc - 1), (int)second(argc), (int)third(argc + 2));
    std::printf("%d %d\n", (int)W::same(p, p), (int)W::same(p, q));
    return 0;
}
