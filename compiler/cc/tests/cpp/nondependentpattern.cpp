// A function template's parameter of a class template's instance that
// names no template parameter, written without a defaulted argument
// (`std::span<const unsigned char>`, its extent defaulted; `S<int>` with a
// value default): in C++ a non-dependent parameter deduces nothing, and
// the argument converts to the instance's parameter type afterwards
// (span<unsigned char> to span<const unsigned char>, an unsigned to a
// variant through its converting constructor), as WebCore's GPUBuffer
// calls `makeArrayBuffer(Variant<std::span<const uint8_t>, size_t>,
// size_t, auto &...)` with `0U` and a span<uint8_t>.
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

inline constexpr size_t seven = 7;
template<typename T, size_t N = seven> struct S { T value; };

struct Wide { int value; };
struct Narrow {
    int value;
    operator Wide() const { return { value * 2 }; }
};

template<typename D> static int f1(S<int> s, D& d) { return s.value + d; }
template<typename D> static int f2(std::span<const unsigned char> s, D& d) { return (int)s.size() + d; }
template<typename D> static int f3(std::span<const unsigned char, std::dynamic_extent> s, D& d) { return (int)s.size() + d; }
template<typename D> static int f4(std::span<const unsigned char> s, D d) { return (int)s[0] + d; }
template<typename D> static int f5(S<Wide> s, D& d) { return s.value.value + d; }

static int makeBuffer(std::variant<std::span<const uint8_t>, size_t> source, size_t offset, auto& cached, auto& device)
{
    cached.push_back(offset);
    return std::visit([](auto value) -> int {
        if constexpr (std::is_same_v<decltype(value), size_t>)
            return (int)value + 100;
        else
            return (int)value.size();
    }, source) + device;
}

int main()
{
    int d = 10;
    S<int> s { 1 };
    unsigned char bytes[4] = { 5, 2, 3, 4 };
    std::span<unsigned char> range(bytes);
    std::span<const unsigned char> crange(bytes);
    printf("%d %d %d %d %d\n", f1(s, d), f2(crange, d), f2(range, d), f3(crange, d), f3(range, d));
    printf("%d %d\n", f4(range, 3), f4(crange, 4L));
    S<Wide> wide { { 3 } };
    printf("%d\n", f5(wide, d));

    std::vector<size_t> cached;
    int device = 1;
    int a = 0, b = 0;
    auto call = [&](auto mappedRange) {
        a = makeBuffer(0U, 0, cached, device);
        b = makeBuffer(mappedRange.first(2), 3, cached, device);
    };
    call(range);
    printf("%d %d %zu\n", a, b, cached.size());
    return 5;
}
