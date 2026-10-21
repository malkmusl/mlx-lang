// C++23's std::optional::transform, and_then and or_else (WebCore's
// FontCustomPlatformData.h: `explicitlyDeclaredWeight.transform([](auto
// range) { return range.maximum; }).value_or(...)`: "no such member"),
// on an lvalue, a const one and an rvalue.
#include <stdio.h>
#include <optional>
#include <string>

struct Range { float minimum; float maximum; };

std::optional<int> half(int x)
{
    if (x % 2)
        return std::nullopt;
    return x / 2;
}

int main(int argc, char **argv)
{
    std::optional<Range> declared { Range { 100, 700 } };
    const std::optional<Range> none;
    float top = declared.transform([](auto range) { return range.maximum; }).value_or(400);
    float fallback = none.transform([](auto range) { return range.maximum; }).value_or(400);
    std::optional<std::string> name = std::optional<int> { 42 }.transform([](int x) { return std::to_string(x); });
    std::optional<int> eight { 8 };
    printf("%g %g %s\n", top, fallback, name->c_str());
    printf("%d %d %d\n", eight.and_then(half).and_then(half).value_or(-1), eight.and_then(half).and_then(half).and_then(half).and_then(half).value_or(-1), std::optional<int> {}.or_else([] { return std::optional<int> { 5 }; }).value());
    return argc;
}
