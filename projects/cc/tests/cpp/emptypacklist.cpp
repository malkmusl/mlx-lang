// A braced list made of an empty pack's expansion for a class with an
// initializer_list constructor (JSC's `m_captures { WriteBarrier<Unknown>(vm,
// this, captures)... }` with no captures): no element, nothing made of
// the pattern.
#include <cstdio>
#include <initializer_list>
#include <vector>

struct Barrier {
    Barrier(int vm, const void *owner, int value) : fValue(vm + value) {}
    int fValue;
};

struct Holder {
    template <typename... Captures> Holder(int vm, Captures... captures) : fCaptures { Barrier(vm, this, captures)... } {}
    std::vector<Barrier> fCaptures;
};

int main(int argc, char **argv) {
    Holder none(1);
    Holder some(10, 1, 2, 3);
    printf("%zu %zu %d\n", none.fCaptures.size(), some.fCaptures.size(), some.fCaptures[2].fValue);
    return argc;
}
