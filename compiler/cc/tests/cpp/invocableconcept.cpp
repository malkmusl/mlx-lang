// WTF's Invocable concept, `requires(F &&f, std::function<Signature>
// expected) { { expected = std::move(f) }; }`: not met by what does not
// take the signature's arguments (the assignment is no hard error: "no
// operator= for these operands" in WebCore's EvaluationMinimum), so the
// overload taking another parameter is chosen.
#include <stdio.h>
#include <functional>
#include <type_traits>
#include <utility>

template<typename Functor, typename Signature> concept Invocable = requires(std::decay_t<Functor> &&f, std::function<Signature> expected) {
    { expected = std::move(f) };
};

struct Zoom { int factor; };

int evaluate(int value, const Invocable<int()> auto &lazy) { return value + lazy(); }
int evaluate(int value, Zoom zoom) { return value * zoom.factor; }

int main(int argc, char **argv)
{
    printf("%d %d\n", (int)Invocable<decltype([] { return 1; }), int()>, (int)Invocable<Zoom, int()>);
    printf("%d %d\n", evaluate(3, [] { return 4; }), evaluate(3, Zoom { 5 }));
    return argc;
}
