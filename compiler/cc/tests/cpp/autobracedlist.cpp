// `auto x = { a, b }` is a std::initializer_list of the elements' type
// (WebCore's `constexpr auto EntityMaskInHTMLAttributeValue = { ... };`),
// at namespace scope and in a function; `auto x{a}` stays a's type. A
// global initializer_list's array lives as long as the program: made in
// the function that makes the globals, it was gone (and `{ 6, 7, 8 }`
// was read as the list's pointer and length) before main.
#include <initializer_list>
#include <stdio.h>
#include <string.h>

enum class Mask : unsigned { Amp = 1, Lt = 2, Gt = 4 };

struct Name {
    char text[8];
    Name(const char *s) { strncpy(text, s, 7); text[7] = 0; }
};

static int f(int x) { return x * 10; }

constexpr auto masks = { Mask::Amp, Mask::Gt };
static auto numbers = { 3, 4, 5 };
const auto names = { Name("one"), Name("two") };
std::initializer_list<int> declared = { 6, 7, 8 };
std::initializer_list<int> computed = { f(1), f(2), f(3) };

static int total(std::initializer_list<int> list)
{
    int sum = 0;
    for (int x : list)
        sum += x;
    return sum;
}

int main(int argc, char **)
{
    unsigned sum = 0;
    for (auto m : masks)
        sum += (unsigned)m;
    auto local = { 1.5, 2.5 };
    double d = 0;
    for (double x : local)
        d += x;
    static const auto kept = { 7L, 8L, 9L };
    auto one = { 42 };
    auto single { 5 };
    printf("%u %zu %g %zu %d %ld\n", sum, masks.size(), d, local.size(), total(numbers), kept.begin()[2]);
    printf("%s %s %zu\n", names.begin()[0].text, names.begin()[1].text, names.size());
    printf("%d %d %d %d\n", total(declared), total(computed), *one.begin() + (int)one.size(), single + 1);
    return argc;
}
