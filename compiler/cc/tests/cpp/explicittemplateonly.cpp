// A call naming a function template with its arguments (a template-id,
// `consume<Normal>(range)`) has the templates of the name for candidates,
// not its plain functions ([over.call.func]). WebCore's font shorthand
// parser calls `consumeIdent<CSSValueNormal>(range)` beside a plain
// `consumeIdent(range)` that takes any keyword: with the plain one
// chosen, `font: bold 12px serif` lost its bold (taken as `normal`).
#include <cstdio>

enum ValueID : unsigned short { Normal = 5, Bold = 7, Italic = 20 };
struct Range { ValueID id; bool used { false }; };

template<typename... emptyBaseCase> bool matches(ValueID) { return false; }
template<ValueID head, ValueID... tail> bool matches(ValueID id) { return id == head || matches<tail...>(id); }

int consume(Range& range) { range.used = true; return 1; }
template<ValueID... names> int consume(Range& range)
{
    if (!matches<names...>(range.id))
        return 0;
    range.used = true;
    return 2;
}
template<typename Predicate> int consume(Range& range, Predicate&& predicate) { return predicate(range.id) ? 3 : 0; }

namespace other {
struct Token { int kind; };
int pick(Token&) { return 10; }
template<int K> int pick(Token& token) { return token.kind == K ? 20 + K : 0; }
}

int main()
{
    Range bold { Bold };
    Range normal { Normal };
    Range italic { Italic };
    int a = consume<Normal>(bold);
    int b = consume<Normal>(normal);
    int c = consume(italic);
    int d = consume<Normal, Bold>(bold);
    int e = consume(bold, [](ValueID id) { return id == Bold; });
    std::printf("%d %d %d %d %d used %d %d %d\n", a, b, c, d, e, bold.used, normal.used, italic.used);
    other::Token token { 2 };
    std::printf("%d %d %d\n", other::pick<2>(token), other::pick<3>(token), other::pick(token));
    return 0;
}
