// An optional of a class made from an rvalue of another, from an
// optional of that other moved (WebCore's generated JSRouterCondition.cpp:
// `std::optional<RouterNotCondition>` of a `std::optional<RouterCondition>&&`,
// RouterNotCondition being made of a `RouterCondition&&` only).
#include <cstdio>
#include <memory>
#include <optional>
#include <string>

struct Condition;

struct NotCondition {
    NotCondition(Condition&&);
    std::unique_ptr<Condition> inner;
};

struct Condition {
    std::string method;
    std::optional<NotCondition> notCondition;
};

NotCondition::NotCondition(Condition&& condition)
    : inner(std::make_unique<Condition>(std::move(condition)))
{
}

struct Wide {
    long value;
    Wide(int v) : value(v * 10) { }
};

int main()
{
    std::optional<Condition> given = Condition { "GET", std::nullopt };
    std::optional<NotCondition> negated = std::move(given);
    printf("negated: %d, method %s\n", negated.has_value(), negated->inner->method.c_str());
    std::optional<Condition> none;
    std::optional<NotCondition> nothing = std::move(none);
    printf("nothing: %d\n", nothing.has_value());
    std::optional<int> number = 4;
    std::optional<Wide> wide = number;
    std::optional<Wide> movedWide = std::optional<int>(7);
    printf("wide %ld %ld\n", wide->value, movedWide->value);
    return 0;
}
