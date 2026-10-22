// Deducing a function pointer parameter from an overloaded function
// name: the overload the pattern matches is taken (CSSNumericType's
// typeFromVector(values, addTypes)).
#include <stdio.h>
#include <optional>
struct Type {
    int units;
    static std::optional<Type> add(const int* values, int count);
    static std::optional<Type> add(Type a, Type b);
};
std::optional<Type> Type::add(Type a, Type b)
{
    if (a.units != b.units)
        return std::nullopt;
    return Type { a.units };
}
template<typename Argument> std::optional<Type> fromAll(const int* values, int count, std::optional<Type> (*function)(Argument, Argument))
{
    std::optional<Type> type = Type { values[0] };
    for (int i = 1; i < count; ++i) {
        type = function(*type, Type { values[i] });
        if (!type)
            return std::nullopt;
    }
    return type;
}
std::optional<Type> Type::add(const int* values, int count)
{
    return fromAll(values, count, add);
}
int main(int argc, char** argv)
{
    int same[] = { 4, 4, 4 };
    int mixed[] = { 4, 5 };
    auto a = Type::add(same, 3);
    auto b = Type::add(mixed, 2);
    printf("%d %d\n", a ? a->units : -1, b ? b->units : -1);
    return argc - 1;
}
