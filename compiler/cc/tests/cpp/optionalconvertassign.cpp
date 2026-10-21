// std::optional<T> assigned another optional<U>, T made from U (WebCore's
// `value = consumeSpecificUnresolvedIdent<CSS::Keyword::Auto>(range);`
// of an optional<MaskBorderWidthValue>: "no operator= for these
// operands"), empty or not, onto an empty or a full one.
#include <stdio.h>
#include <optional>

struct Auto { int v; };
struct Width {
    int kind;
    Width(Auto a) : kind(a.v * 10) {}
    Width(int n) : kind(n) {}
};

std::optional<Auto> consume(bool present) { if (present) return Auto { 4 }; return std::nullopt; }

int main(int argc, char **argv)
{
    std::optional<Width> value;
    value = consume(false);
    printf("%d\n", (int)value.has_value());
    value = consume(true);
    printf("%d %d\n", (int)value.has_value(), value->kind);
    const std::optional<Auto> other { Auto { 7 } };
    value = other;
    printf("%d\n", value->kind);
    value = consume(false);
    printf("%d\n", (int)value.has_value());
    value = 3;
    printf("%d\n", value->kind);
    return argc;
}
