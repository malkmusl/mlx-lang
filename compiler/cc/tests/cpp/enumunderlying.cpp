// An enum's underlying type is the one given, a type of its own: `bool`
// (WTF's isValidEnumForPersistence overloads on `std::same_as<
// std::underlying_type_t<E>, bool>`), `signed char`, `long long`; the
// default ones by the enumerators; make_signed and make_unsigned of an
// enum the integers of its size.
#include <stdio.h>
#include <stdint.h>
#include <type_traits>

enum class Flag : bool { No, Yes };
enum class Small : signed char { A = -1 };
enum class Byte : uint8_t { B };
enum class Wide : unsigned long long { C };
enum class Plain { D };
enum class Long : long { E };

template<typename E> requires (!std::is_same_v<std::underlying_type_t<E>, bool>) int kind(std::underlying_type_t<E> v) { return 1 + (int)v; }
template<typename E> requires (std::is_same_v<std::underlying_type_t<E>, bool>) int kind(bool v) { return 10 + v; }

int main(int argc, char **argv)
{
    printf("%d %d %d %d\n", (int)std::is_same_v<std::underlying_type_t<Flag>, bool>, (int)std::is_same_v<std::underlying_type_t<Small>, signed char>, (int)std::is_same_v<std::underlying_type_t<Wide>, unsigned long long>, (int)std::is_same_v<std::underlying_type_t<Byte>, unsigned char>);
    printf("%d %d %d\n", (int)std::is_same_v<std::underlying_type_t<Plain>, int>, (int)std::is_same_v<std::underlying_type_t<Long>, long>, (int)sizeof(Flag));
    printf("%d %d %d\n", (int)std::is_same_v<std::make_signed_t<Wide>, long>, (int)std::is_same_v<std::make_unsigned_t<Small>, unsigned char>, (int)std::is_same_v<std::make_unsigned_t<Plain>, unsigned>);
    printf("%d %d %d\n", kind<Flag>(true), kind<Small>(-1), static_cast<std::underlying_type_t<Small>>(Small::A));
    return argc;
}
