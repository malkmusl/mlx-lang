// A data member hides a base's member function of the same name: `size`
// unqualified in a member function (inline or out of line) is the data
// member (WTF::ConcurrentPtrHashSet's Table: `unsigned size;` beside
// TrailingArray's `size()`; its maxLoad() was the function's address).
#include <stdio.h>

struct Trailing { unsigned size() const { return m_size; } unsigned m_size = 7; };
struct Table final : Trailing {
    unsigned size;
    unsigned mask;
    unsigned maxLoad() const { return size / 2; }
    unsigned outOfLine() const;
};
unsigned Table::outOfLine() const { return size + mask; }

struct Middle : Trailing { int size = 5; };
struct Leaf : Middle { int twice() const { return size * 2; } };

// A function of the derived class still hides a base's data member.
struct Data { int count = 3; };
struct Counted : Data { int count() const { return 11; } int get() const { return count(); } };

static volatile unsigned limit = 16;

int main() {
    Table t;
    t.size = 32;
    t.mask = 31;
    printf("%u %u %d\n", t.maxLoad(), t.outOfLine(), limit >= t.maxLoad());
    printf("%u\n", static_cast<Trailing&>(t).size());
    Leaf l;
    printf("%d\n", l.twice());
    Counted c;
    printf("%d\n", c.get());
    return 0;
}
