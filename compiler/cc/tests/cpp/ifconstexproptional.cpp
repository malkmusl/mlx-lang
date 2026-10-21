// `if constexpr` on a std::optional a consteval function returns, by
// itself or declared in the condition (`if constexpr (constexpr auto
// index = analogousComponentIndex<Output, Input, I>())`, WebCore's
// ColorConversion.h): the optional's conversion to bool, the result
// moved into the constant in a constant expression.
#include <optional>
#include <stdio.h>

struct Info { std::optional<int> category; };
struct ModelA { static constexpr Info componentInfo[4] { { 1 }, { 2 }, { std::nullopt }, { 4 } }; };
struct ModelB { static constexpr Info componentInfo[4] { { 2 }, { 1 }, { 3 }, { 4 } }; };
struct A { using Model = ModelA; };
struct B { using Model = ModelB; };

template<typename Output, typename Input, unsigned I>
consteval std::optional<unsigned> analogous()
{
    if constexpr (I == 3)
        return 3;
    else if constexpr (constexpr auto c = Input::Model::componentInfo[I].category) {
        if constexpr (*c == Output::Model::componentInfo[0].category)
            return 0;
        else if constexpr (*c == Output::Model::componentInfo[1].category)
            return 1;
        else
            return std::nullopt;
    } else
        return std::nullopt;
}

template<typename Output, typename Input, unsigned I>
constexpr void carry(const float *input, float *output)
{
    if constexpr (constexpr auto index = analogous<Output, Input, I>()) {
        if (input[I] < 0)
            output[*index] = -1 - (float)I;
    }
}

consteval std::optional<unsigned> pick(int i)
{
    if (i == 3)
        return 3;
    return std::nullopt;
}

int main(int argc, char **)
{
    float in[4] { -1, -1, -1, -1 };
    float out[4] { 0, 0, 0, 0 };
    carry<B, A, 0>(in, out);
    carry<B, A, 1>(in, out);
    carry<B, A, 2>(in, out);
    carry<B, A, 3>(in, out);
    printf("%g %g %g %g\n", out[0], out[1], out[2], out[3]);
    if constexpr (pick(3))
        printf("three\n");
    if constexpr (pick(2))
        printf("two\n");
    constexpr auto kept = pick(3);
    if constexpr (kept)
        printf("kept %u\n", *kept);
    if constexpr (constexpr std::optional<unsigned> given = 5)
        printf("given %u\n", *given);
    return argc;
}
