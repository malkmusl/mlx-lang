// A scoped enumeration converts implicitly to no integer type
// ([conv.prom] promotes only unscoped ones): an overload taking its
// underlying type is not viable, a constrained template for enums is
// (WTF::Persistence::Encoder's `operator<<(const T &) requires
// std::is_enum_v<T>` beside its `operator<<(uint8_t)`: KeyedEncoderGeneric
// wrote `Type::String` as one byte where the decoder reads eight, and
// every IndexedDB key path read back from SQLite failed to decode).
#include <stdint.h>
#include <stdio.h>
#include <type_traits>

struct Encoder {
    template<typename T>
        requires(std::is_enum_v<T>)
    Encoder& operator<<(const T& value)
    {
        printf("enum ");
        return *this << static_cast<uint64_t>(value);
    }
    Encoder& operator<<(uint8_t value)
    {
        printf("u8 %u\n", value);
        return *this;
    }
    Encoder& operator<<(uint64_t value)
    {
        printf("u64 %llu\n", static_cast<unsigned long long>(value));
        return *this;
    }
};

enum class Type : uint8_t { Bytes, Bool, String };
enum Plain : uint8_t { PlainZero, PlainOne };

static void take(int) { printf("take int\n"); }
template<typename T> static void take(T) { printf("take template\n"); }

static void pick(double) { printf("pick double\n"); }
static void pick(Type) { printf("pick Type\n"); }

int main()
{
    Encoder encoder;
    encoder << Type::String;
    Type type = Type::Bool;
    encoder << type;
    encoder << static_cast<uint8_t>(4);
    take(Type::Bytes);
    take(PlainOne);
    pick(Type::String);
    printf("convertible %d %d constructible %d\n", std::is_convertible_v<Type, int>, std::is_convertible_v<Plain, int>, std::is_constructible_v<int, Type>);
    return 0;
}
