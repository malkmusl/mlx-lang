// The size of a local std::array, a constant though the array is not
// (WebCore's CSSRegisteredCounterStyle: `std::array<char16_t,
// groups.size() * 3> buffer;`), and a constexpr member that reads a local
// object, which is no constant and runs.
#include <array>
#include <cstdio>

struct Counter {
    int value;
    constexpr int get() const { return value; }
    static constexpr int width() { return 4; }
    constexpr int capacity() const { return 8; }
};

int main(int argc, char**)
{
    std::array<unsigned char, 5> groups;
    for (auto& group : groups)
        group = argc;
    std::array<char16_t, groups.size() * 3> buffer { };
    printf("buffer %zu, group %d\n", buffer.size(), groups[0]);

    Counter counter { argc * 10 };
    int read = counter.get();
    std::array<int, counter.capacity()> slots { };
    printf("read %d, slots %zu\n", read, slots.size());
    return 0;
}
