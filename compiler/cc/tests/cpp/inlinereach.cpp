// Inline functions, static functions and vtables are made only where the
// code or data made refers to them, as C++ has the inline functions of
// headers made where they are used: an inline function named only by
// another inline function that is never made is left out, and a class's
// vtable and virtual functions are made only when a constructor made
// stores the vtable. (mlx-cc made every one a parsed expression named:
// WebCore's prefix header alone gave each object 15,000 functions.)
// tools/check_cpp.sh also reads the object's symbols.
#include <cstdio>

inline int tripled(int x) { return x * 3; }
inline int unusedInline(int x) { return tripled(x) + 1; }

struct Unused {
    virtual ~Unused() { }
    virtual int value() { return tripled(2); }
};
inline Unused* makeUnused() { return new Unused; }

struct Used {
    virtual ~Used() { }
    virtual int value() { return 7; }
};

inline int doubled(int x) { return 2 * x; }
static int helper(int x) { return doubled(x); }
// (Reached through data: the table's address of helper.)
static int (*const table[])(int) = { helper };

template<typename T> T halved(T x) { return x / 2; }
inline int quartered(int x) { return halved(halved(x)); }

int main()
{
    Used used;
    Used* pointer = &used;
    printf("%d %d %d\n", pointer->value(), table[0](21), quartered(40));
    return 0;
}
