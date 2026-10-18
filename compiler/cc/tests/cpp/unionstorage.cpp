// A union's constructor and destructor of its own leave its members
// alone: a member of a class with no default constructor is not made,
// nor destroyed (Skia's THashTable slot storage, union Storage { T
// fStorage; Storage() {} ~Storage() {} }).
#include <cstdio>
#include <new>
#include <utility>

static int made = 0;
static int destroyed = 0;

struct Font {
    explicit Font(int id) : id(id) { made += 1; }
    Font(Font &&other) : id(other.id) { made += 1; }
    ~Font() { destroyed += 1; }
    int id;
};

template <typename T>
struct Slot {
    Slot() {}
    ~Slot() { reset(); }
    void emplace(T &&value) {
        new (&fVal.fStorage) T(std::move(value));
        full = true;
    }
    void reset() {
        if (full) fVal.fStorage.~T();
        full = false;
    }
    bool full = false;
    union Storage {
        T fStorage;
        Storage() {}
        ~Storage() {}
    } fVal;
};

int main(int argc, char **argv) {
    {
        Slot<Font> slots[3];
        printf("%d %d\n", made, destroyed);
        slots[1].emplace(Font(7));
        printf("%d %d %d\n", made, destroyed, slots[1].fVal.fStorage.id);
    }
    printf("%d %d\n", made, destroyed);
    return argc;
}
