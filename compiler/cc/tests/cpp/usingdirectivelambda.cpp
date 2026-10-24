// A using-directive of a function seen in its generic lambdas' instances,
// made once the function is read (WebCore's StyleFontFamily: `using
// namespace CSSPropertyParserHelpers;` then a lambda given to
// compactMap calling genericFontFamily), through a lambda in it too.
#include <cstdio>
#include <vector>
namespace Helpers {
int generic(int id) { return id * 10; }
}
namespace Style {
template<typename F> std::vector<int> mapAll(const std::vector<int>& v, F f)
{
    std::vector<int> out;
    for (auto& x : v)
        out.push_back(f(x));
    return out;
}
int convert(const std::vector<int>& values)
{
    using namespace Helpers;
    auto first = generic(1);
    auto mapped = mapAll(values, [&](auto& value) {
        auto [a, b] = [&] -> std::pair<int, int> {
            return { generic(value), first };
        }();
        return a + b;
    });
    return mapped[0] + mapped[1];
}
}
int main() {
    std::printf("%d\n", Style::convert({ 2, 3 }));
    return 0;
}
