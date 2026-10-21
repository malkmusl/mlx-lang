// `x = { a, b, c }` of a class with an initializer_list constructor takes
// that constructor, as list-initialization does, though another
// constructor's class (std::optional<E>) could be made of the braces too
// (WebCore's `hitType = { ReadOnly, Active, AllowChildFrameContent }` of
// an OptionSet<HitTestRequest::Type>).
#include <stdio.h>
#include <initializer_list>
#include <optional>
enum class Type : unsigned { ReadOnly = 1, Active = 2, Child = 4 };
struct Set {
    unsigned storage { 0 };
    constexpr Set() = default;
    constexpr Set(Type t) : storage((unsigned)t) { }
    constexpr Set(std::optional<Type> t) : storage(t ? (unsigned)*t : 0) { }
    constexpr Set(std::initializer_list<Type> list) { for (auto t : list) storage |= (unsigned)t; }
};
int main(int argc, char**)
{
    Set hitType;
    if (argc)
        hitType = { Type::ReadOnly, Type::Active, Type::Child };
    Set other;
    other = { Type::Active };
    printf("%u %u\n", hitType.storage, other.storage);
    return argc;
}
