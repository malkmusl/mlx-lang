// C++17's using-declarations of several names (`using SkPDFUtils::EmptyPath,
// SkPDFUtils::EmptyVerb, SkPDFUtils::EmptyArea;`), in a block and in a
// namespace.
#include <cstdio>

namespace Utils {
enum class EmptyPath { Discard, Keep };
enum class EmptyArea { Discard, Keep };
inline int emit(EmptyPath p, EmptyArea a) { return (int)p * 10 + (int)a; }
inline int twice(int x) { return 2 * x; }
}

namespace Other {
using Utils::twice, Utils::emit;
}

int main(int argc, char **argv) {
    using Utils::EmptyPath, Utils::EmptyArea;
    const EmptyArea area = argc > 5 ? EmptyArea::Discard : EmptyArea::Keep;
    printf("%d %d\n", Other::emit(EmptyPath::Keep, area), Other::twice(21));
    return argc;
}
