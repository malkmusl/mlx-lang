// Generic lambdas converted to pointers to functions whose parameters are
// references: each `auto` parameter is the pointer's parameter type as it
// is ([temp.deduct.funcaddr]), in a constant table too (WebCore's
// LinkRelAttribute linkTypes: `[](auto) { return true; }` as a
// `bool (*)(const Document &)`; the closure's address was taken instead).
#include <cstdio>
struct Doc { bool flag; };
struct Rel { bool alternate { false }; };
struct Details { bool (*isEnabled)(const Doc&); void (*updateRel)(Rel&); };
static constexpr Details plainTable[1] = { { [](const Doc&) { return true; }, [](Rel& rel) { rel.alternate = true; } } };
static constexpr Details genericTable[1] = { { [](auto) { return true; }, [](auto rel) { rel.alternate = true; } } };
int main(int argc, char **)
{
    Doc d { true };
    bool (*runtime)(const Doc&) = [](auto document) { return document.flag; };
    printf("runtime %d\n", runtime(d));
    Rel r;
    printf("plain %d\n", plainTable[0].isEnabled(d));
    plainTable[0].updateRel(r);
    printf("plain rel %d\n", r.alternate);
    printf("generic pointer %d\n", genericTable[0].isEnabled != nullptr);
    printf("generic %d\n", genericTable[0].isEnabled(d));
    Rel s;
    genericTable[0].updateRel(s);
    printf("generic rel %d\n", s.alternate);
    return argc == 2 ? 0 : 1;
}
