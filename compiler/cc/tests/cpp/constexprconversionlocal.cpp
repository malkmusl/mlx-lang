// A constexpr integer local initialized by a class's constexpr
// conversion (`constexpr bool isSigned = std::is_signed<IntType>();` in
// JavaScriptCore's WasmBBQJIT64.h) is a constant `if constexpr` and
// array bounds read: the conversion computed at compile time.
#include <stdint.h>
#include <stdio.h>
#include <type_traits>

struct Flag {
    bool on;
    constexpr operator bool() const { return on; }
};

struct Count {
    int n;
    constexpr Count(int v) : n(v * 2) { }
    constexpr operator int() const { return n + 1; }
};

template<typename IntType, bool IsMod>
int pick()
{
    constexpr bool isSigned = std::is_signed<IntType>();
    constexpr bool is32 = sizeof(IntType) == 4;
    if constexpr (isSigned) {
        if constexpr (is32)
            return 1;
        else
            return 2;
    }
    return IsMod ? 3 : 4;
}

int main()
{
    constexpr bool off = Flag { false };
    constexpr int size = Count(3);
    char buffer[size];
    int flags = 0;
    if constexpr (off)
        flags = 100;
    if constexpr (!std::is_unsigned<int>())
        flags += 10;
    printf("%d %d %d %d %d %d\n", pick<int32_t, true>(), pick<int64_t, false>(), pick<uint32_t, true>(), pick<uint64_t, false>(), (int)sizeof(buffer), flags);
    return 0;
}
