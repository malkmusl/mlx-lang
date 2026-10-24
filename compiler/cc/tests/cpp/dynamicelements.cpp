// Elements and members of a global initialized from values that are no
// constants (variables, calls): set when the program starts (Skia's
// SkOpts.cpp `StageFn ops_lowp[] = { (StageFn)lowp::stage, ... }` of
// static function pointer variables).
#include <cstdio>

typedef void (*StageFn)(void);

static void one() { printf("one\n"); }
static void two() { printf("two\n"); }

namespace lowp {
static void (*first)(void) = nullptr;
static void (*second)(void) = two;
}

static int counter() { return 41; }
static int base = 7;

StageFn stages[] = { (StageFn)one, (StageFn)lowp::first, (StageFn)lowp::second };

struct Entry {
    const char *name;
    int value;
};

Entry entries[] = { {"constant", 3}, {"call", counter()}, {"variable", base} };
int numbers[3] = { 1, counter() + 1, base * 2 };

int main(int argc, char **argv) {
    for (StageFn fn : stages) {
        if (fn) {
            fn();
        } else {
            printf("null\n");
        }
    }
    for (const Entry &e : entries) printf("%s %d\n", e.name, e.value);
    printf("%d %d %d\n", numbers[0], numbers[1], numbers[2]);
    return argc;
}
