// std::ranges::transform of a range into an output iterator (WebCore's
// MIMETypeRegistry: a std::array of literals into a FixedVector), and
// where the input and the output stopped.
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

struct Literal {
    const char* text;
};

int main()
{
    std::array<Literal, 3> literals { { { "image/png" }, { "image/jpeg" }, { "image/webp" } } };
    std::vector<const char*> texts(literals.size());
    std::ranges::transform(literals, texts.begin(), [](auto literal) {
        return literal.text;
    });
    for (auto* text : texts)
        printf("%s\n", text);

    std::vector<int> numbers { 1, 2, 3, 4 };
    int squares[4] = { };
    auto result = std::ranges::transform(numbers, squares, [](int n) { return n * n; });
    printf("%d %d %d %d, in at end: %d, out at %d\n", squares[0], squares[1], squares[2], squares[3], result.in == numbers.end(), static_cast<int>(result.out - squares));
    return 0;
}
