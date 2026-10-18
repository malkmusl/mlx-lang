// Forms of Skia's: a declaration as a for loop's condition (`for (auto
// iter = raw.iter(); auto rec = iter.next();)`), an array of function
// pointers made of lambdas, a class template's arguments deduced from a
// braced list (`std::tuple keyData = {a, b}`).
#include <cstdio>
#include <cstdint>
#include <optional>
#include <tuple>

struct Iter {
    int at = 0;
    int end = 3;
    std::optional<int> next() {
        if (at == end) return std::nullopt;
        return at++ * 10;
    }
};

int main(int argc, char **argv) {
    int sum = 0;
    for (Iter iter; auto rec = iter.next();) { sum += *rec; }
    int steps = 0;
    for (int i = 0; int left = 3 - i; ++i) { steps += left; }

    using MemSet = void (*)(void *, uint64_t c, int count);
    const MemSet procs[] = {
        [](void *addr, uint64_t c, int count) {
            for (int i = 0; i < count; i++) ((uint8_t *)addr)[i] = (uint8_t)c;
        },
        [](void *addr, uint64_t c, int count) {
            for (int i = 0; i < count; i++) ((uint16_t *)addr)[i] = (uint16_t)c;
        },
    };
    uint16_t buffer[4] = {};
    procs[1](buffer, 0x102, 3);
    procs[0](buffer, 7, 1);

    std::tuple keyData = {sum, 2.5, 'k'};
    printf("%d %d %x %x %d %.1f %c\n", sum, steps, buffer[0], buffer[2], std::get<0>(keyData), std::get<1>(keyData), std::get<2>(keyData));
    return argc;
}
