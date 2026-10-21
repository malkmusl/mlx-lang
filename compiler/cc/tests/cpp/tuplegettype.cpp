// std::get<T> of a tuple by its element's type (WebCore's
// SVGFilterRenderer.cpp: `std::get<FilterEffectVector>(*result)` of an
// optional tuple: "no instance of the function template 'get'"), beside
// get<I>; of a const tuple and of an rvalue.
#include <stdio.h>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

struct Expression { int steps; };

std::optional<std::tuple<Expression, std::vector<int>>> build()
{
    return std::tuple<Expression, std::vector<int>> { Expression { 3 }, std::vector<int> { 4, 5 } };
}

int main(int argc, char **argv)
{
    auto result = build();
    auto &expression = std::get<Expression>(*result);
    auto &effects = std::get<std::vector<int>>(*result);
    const std::tuple<int, std::string, char> constant { 1, "two", 'c' };
    std::string moved = std::get<std::string>(std::tuple<int, std::string> { 9, "nine" });
    printf("%d %zu %d\n", expression.steps, effects.size(), std::get<1>(*result)[1]);
    printf("%d %s %c %s\n", std::get<int>(constant), std::get<std::string>(constant).c_str(), std::get<char>(constant), moved.c_str());
    return argc;
}
