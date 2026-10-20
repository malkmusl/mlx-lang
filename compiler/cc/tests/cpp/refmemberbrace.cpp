// A reference member given `{ }` in a designated initializer binds to a
// temporary of its class (CSSCalc::EvaluationOptions' symbolTable).
#include <stdio.h>
struct Symbols { int size = 7; };
struct Options {
    int range;
    const Symbols& symbols;
};
int sizeOf(const Options& options) { return options.symbols.size; }
int main(int argc, char** argv)
{
    auto options = Options { .range = 3, .symbols = { } };
    printf("%d %d\n", options.range, sizeOf(options));
    return argc - 1;
}
