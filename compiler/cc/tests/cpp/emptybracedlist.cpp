// `{ }` for a class with no default constructor but one taking an
// initializer_list: an empty list (WebCore's CSSParserToken::serialize,
// `appendCommentIfNeeded({ }, '*')` of a generic lambda).
#include <array>
#include <cstdio>
#include <initializer_list>

enum TokenType { Ident, Function, Number, Count };

struct Builder {
    constexpr Builder(std::initializer_list<TokenType> tokens)
    {
        for (auto token : tokens)
            buffer[token] = true;
    }
    std::array<bool, Count> buffer { false };
};

int main()
{
    int calls = 0;
    auto append = [&](const Builder& tokens, auto... delimiters) {
        calls++;
        printf("ident %d number %d delimiters %zu\n", tokens.buffer[Ident], tokens.buffer[Number], sizeof...(delimiters));
    };
    append({ Ident, Number }, '-');
    append({ }, '*');
    append({ Function });
    printf("calls %d\n", calls);
    return 0;
}
