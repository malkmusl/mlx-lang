// Static local references (`static SkMutex &mutex = *(new SkMutex);`,
// Skia's SkTypefaceCache.cpp): bound once, the first time the
// declaration runs; in an inline function, one for the program.
#include <cstdio>

struct Mutex {
    static int made;
    int locks = 0;
    Mutex() { ++made; }
};
int Mutex::made = 0;

static Mutex &cacheMutex() {
    static Mutex &mutex = *(new Mutex);
    return mutex;
}

inline const int &stub(bool fresh) {
    static int value = 41;
    static const int &ref(value);
    if (fresh) ++value;
    return ref;
}

int main(int argc, char **argv) {
    cacheMutex().locks++;
    cacheMutex().locks++;
    static const Mutex &again = cacheMutex();
    printf("%d %d %d\n", Mutex::made, cacheMutex().locks, again.locks);
    const int before = stub(false);
    const int after = stub(true);
    printf("%d %d\n", before, after);
    return argc;
}
