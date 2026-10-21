/* C23 as mlx-cc reads it with -std=c23: the keywords bool, true, false,
   nullptr, static_assert, alignas, alignof, thread_local, typeof,
   typeof_unqual and constexpr, auto with its type inferred,
   [[attributes]], digit separators, #elifdef and #elifndef, enums with a
   fixed underlying type, empty initializers, unnamed parameters, labels
   before declarations and at the end of a block. gcc -std=c2x printed
   the expected output. */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef NOTHING
#error no
#elifdef __STDC_VERSION__
#define STAGE "elifdef"
#else
#error no
#endif

#ifndef __STDC_VERSION__
#error no
#elifndef __STDC_VERSION__
#error no
#elifndef SOMETHING_ELSE
#define STAGE2 "elifndef"
#endif

static_assert(sizeof(int) == 4);
static_assert(__STDC_VERSION__ >= 202000L, "C23");
static_assert(true == 1 && false == 0, "booleans");
static_assert(sizeof(true) == sizeof(bool), "true is a bool");

enum Small : unsigned char { S_ONE = 1, S_MAX = 255 };
enum Wide : long long { W_BIG = 1LL << 40 };

constexpr int LIMIT = 10;
constexpr unsigned long long SHIFTED = 1ULL << 40;
static int table[LIMIT];

struct Point { int x, y; };

[[nodiscard]] static int twice(int value) { return value * 2; }
[[maybe_unused]] static int unused_helper(int, int second) { return second; }
[[deprecated("old")]] static int old_name(void) { return 7; }

static int classify(int n)
{
    switch (n) {
    case 0:
        n += 100;
        [[fallthrough]];
    case 1:
        return n + 1;
    default:
        return n;
    }
}

int main(void)
{
    alignas(16) char buffer[32] = {};
    struct Point origin = {};
    int zeroes[4] = {};
    auto count = 1'000'000;
    auto ratio = 0x1'0000p-16;
    auto text = "digits";
    typeof(count) more = count + 0b1'0000;
    typeof_unqual(const int) plain = 3;
    bool flag = true;
    int *nothing = nullptr;
    nullptr_t none = nullptr;
    thread_local static int counter = 5;
    [[maybe_unused]] int unused = 0;

    printf("%s %s\n", STAGE, STAGE2);
    printf("%d %d %zu %zu\n", flag, !flag, sizeof(bool), sizeof(flag));
    printf("%d %d %d\n", nothing == nullptr, none == nullptr, nothing == NULL);
    printf("%d %d %g %s\n", count, more, ratio, text);
    printf("%zu %zu %d\n", sizeof(count), sizeof(text), plain);
    printf("%zu %zu %d %lld\n", sizeof(enum Small), sizeof(enum Wide), S_MAX, (long long)W_BIG);
    printf("%d %llu %zu\n", LIMIT, SHIFTED, sizeof table / sizeof table[0]);
    printf("%d %d %d %d\n", buffer[0], origin.x + origin.y, zeroes[3], counter);
    printf("%zu %d\n", alignof(max_align_t), (int)((uintptr_t)buffer % 16));
    printf("%d %d %d\n", twice(21), unused_helper(1, 2), classify(0));
    printf("%d %d %d\n", classify(1), classify(5), old_name());
    {
        int n = 0;
    again:
        int step = n + 1;
        n = step;
        if (n < 3) goto again;
        printf("%d\n", n);
    done:
    }
    return 0;
}
