// A local enum of a function named in a lambda written in another
// lambda (WebCore's SelectorChecker::matchHasArgumentSelector: its
// HasTraversalType in the filter lambda's type lambda): the inner body
// sees the types of the function around the outer one.
#include <cstdint>
#include <cstdio>
#include <optional>

enum class Kind : uint8_t { Children, Descendants };

static int f(int which)
{
    enum class TraversalType : uint8_t { Children, Descendants, Siblings };
    auto traversalType = [&] {
        if (which == 0)
            return TraversalType::Children;
        if (which == 1)
            return TraversalType::Descendants;
        return TraversalType::Siblings;
    }();
    auto filter = [&]() -> int {
        auto type = [&]() -> std::optional<Kind> {
            switch (traversalType) {
            case TraversalType::Children:
                return Kind::Children;
            case TraversalType::Descendants:
                return Kind::Descendants;
            default:
                return { };
            }
        }();
        return type ? static_cast<int>(*type) : -1;
    };
    return filter();
}

int main()
{
    printf("%d %d %d\n", f(0), f(1), f(2));
    return 0;
}
