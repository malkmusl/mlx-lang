// A function pointer template parameter defaulted to a null pointer
// (Skia's SkTDPQueue: `int *(*INDEX)(const T &) = (int *(*)(const T &))nullptr`).
#include <cstdio>

template <typename T, bool (*LESS)(const T &, const T &), int *(*INDEX)(const T &) = (int *(*)(const T &))nullptr>
class Queue {
public:
    bool less(const T &a, const T &b) const { return LESS(a, b); }
    bool indexed() const { return INDEX != nullptr; }
    int *index(const T &t) const { return INDEX ? INDEX(t) : nullptr; }
};

struct Item { int key; int slot; };
static bool lessItem(const Item &a, const Item &b) { return a.key < b.key; }
static int *slotOf(const Item &i) { return const_cast<int *>(&i.slot); }

int main(int argc, char **argv) {
    Queue<Item, lessItem> plain;
    Queue<Item, lessItem, slotOf> withIndex;
    Item a{1, 7}, b{2, 8};
    printf("%d %d %d %d %d\n", plain.less(a, b), plain.indexed(), withIndex.indexed(), *withIndex.index(b), plain.index(a) == nullptr);
    return argc;
}
