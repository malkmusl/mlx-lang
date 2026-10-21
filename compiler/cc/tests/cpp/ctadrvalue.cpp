// In a constructor's implicit deduction guide, `Ts&&` of the class's own
// parameters is no forwarding reference: lvalues deduce through the
// `const Ts&...` constructor, the types without references (WebCore's
// `SpaceSeparatedTuple { box, length }`).
#include <stdio.h>
#include <tuple>
#include <type_traits>
#include <utility>

template<typename... Ts> struct SpaceSeparatedTuple {
    constexpr SpaceSeparatedTuple(Ts&&... values) : value { std::make_tuple(std::forward<Ts>(values)...) }, moved(true) { }
    constexpr SpaceSeparatedTuple(const Ts&... values) : value { std::make_tuple(values...) }, moved(false) { }
    std::tuple<Ts...> value;
    bool moved;
};

template<class T> struct Box {
    template<class U> Box(U &&u) : value(std::forward<U>(u)) { }
    T value;
};

struct VisualBox { int kind; };
struct Length { float value; };

int main(int argc, char **argv)
{
    VisualBox box { 2 };
    Length length { 1.5f };
    SpaceSeparatedTuple fromLvalues { box, length };
    SpaceSeparatedTuple fromRvalues { VisualBox { 3 }, Length { 4.5f } };
    static_assert(std::is_same_v<decltype(fromLvalues), SpaceSeparatedTuple<VisualBox, Length>>);
    static_assert(std::is_same_v<decltype(fromRvalues), SpaceSeparatedTuple<VisualBox, Length>>);
    Box<int> forwarded { argc };
    printf("%d %d %d %d %d\n", std::get<0>(fromLvalues.value).kind, (int)fromLvalues.moved, (int)fromRvalues.moved, (int)(std::get<1>(fromRvalues.value).value * 2), forwarded.value);
    return argc - 2;
}
