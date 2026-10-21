// A range-based for over a braced list of functions: the element is a
// pointer to function (the list's type decays them), called through the
// loop variable (Skia's SkConvertPixels: `for (auto fn : {rect_memcpy,
// swizzle_or_premul, convert_to_alpha8})` called a function named fn).
#include <cstdio>
#include <initializer_list>

static bool small(int v, int* out) { if (v < 10) { *out = 1; return true; } return false; }
static bool medium(int v, int* out) { if (v < 100) { *out = 2; return true; } return false; }
static bool large(int, int* out) { *out = 3; return true; }

static int classify(int v)
{
    int result = 0;
    for (auto fn : { small, medium, large }) {
        if (fn(v, &result))
            break;
    }
    return result;
}

int main()
{
    std::printf("%d %d %d\n", classify(5), classify(50), classify(500));
    int sum = 0;
    for (auto* f : { small, large }) {
        int r = 0;
        f(7, &r);
        sum += r;
    }
    std::printf("%d\n", sum);
    return 0;
}
