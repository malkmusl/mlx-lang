// A designated braced argument converts only to a class with the member
// it names: of WebCore's MetaConsumer overloads, `consume(range, state,
// { .overrideParserMode = HTMLStandardMode })` is the template taking
// the options, not the function taking a CSSCalcSymbolsAllowed first
// ("expected an expression" reading the list as one).
#include <stdio.h>
#include <utility>

enum Mode { Quirks, Standard };
struct Options { int zero = 1; Mode overrideParserMode = Quirks; };
struct Symbols { int count = 0; };
struct Range { int v; };

struct Meta {
    template<typename... F> static int consume(Range &r, int state, Symbols symbols, Options options, F &&...f) { return r.v + state + symbols.count * 1000 + options.overrideParserMode * 10 + options.zero * 100 + (int)sizeof...(f); }
    template<typename... F> static int consume(Range &r, int state, Options options, F &&...f) { return consume(r, state, { }, options, std::forward<F>(f)...); }
    static int consume(Range &r, int state, Symbols symbols = { }, Options options = { }) { return consume(r, state, symbols, options, 0) + 10000; }
};

int main(int argc, char **argv)
{
    Range r { 3 };
    printf("%d\n", Meta::consume(r, 1, { .overrideParserMode = Standard }));
    printf("%d\n", Meta::consume(r, 1, { .count = 2 }));
    printf("%d\n", Meta::consume(r, 1));
    return argc;
}
