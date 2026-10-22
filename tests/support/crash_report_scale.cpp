// tools/check_crash_report.sh: a C++ program that crashes as webcore-dump
// did, for the crash report at scale: 21 calls deep through functions
// with long template names, a store through a null pointer at the bottom.
// The check links it with an object of very many functions (generated),
// so that the symbol table is large; the report must name every frame,
// mangled and as written in C++, and take no more of the C library's
// value arena than a frame's naming does.
#include <stdio.h>

namespace scale {
template <typename T, int N> struct Nest { T value; int depth; };
template <typename T> struct Box { T inner; };

typedef Nest<Box<Nest<Box<int>, 1> >, 2> Deep;

template <int N> struct Level {
    static int down(FILE *sink, Nest<Box<Deep>, N> *at, long turns) {
        Nest<Box<Deep>, N - 1> next = { { at->value.inner }, at->depth + 1 };
        return Level<N - 1>::down(sink, &next, turns) + 1;
    }
};

template <> struct Level<0> {
    static int down(FILE *sink, Nest<Box<Deep>, 0> *at, long turns) {
        fprintf(sink, "%d", at->depth);
        int *nowhere = (int *)(turns - turns);
        *nowhere = at->depth;
        return at->depth;
    }
};
}

extern "C" int many_functions(int);

int main(int argc, char **argv) {
    FILE *sink = fopen("/dev/null", "w");
    if (!sink) return 2;
    scale::Nest<scale::Box<scale::Deep>, 20> top = { {}, many_functions(argc) };
    return scale::Level<20>::down(sink, &top, argc);
}
