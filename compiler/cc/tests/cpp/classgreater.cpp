// `a > b` and `a >= b` of a class operand without an operator function:
// the built-in operator on what the class converts to, its operands
// swapped (`b < a`), as for `<` (WTF's `CheckedSize > MAX_ARRAY_BUFFER_SIZE`
// compared the object itself, too big to be a value: new
// Float64Array(100000) was out of memory).
#include <stdio.h>
#include <stddef.h>

struct RecordOverflow {
    bool m_overflowed = false;
    bool hasOverflowed() const { return m_overflowed; }
};

template<typename T, typename H = RecordOverflow> class Checked : public H {
public:
    Checked(T v) : m_value(v) {}
    operator T() const { return m_value; }
private:
    T m_value;
};

struct Wide {
    long pad[3];
    int v;
    operator int() const { return v; }
};

int main(int argc, char **) {
    Checked<size_t> size = 800000;
    size_t limit = 1ull << 32;
    printf("%d %d %d %d\n", size > limit, size >= limit, size < limit, size <= limit);
    printf("%d %d %d\n", size > 4ull, size > 800000ull, size >= 800000ull);
    printf("%d %d\n", limit > size, limit >= size);
    Wide w = {{0, 0, 0}, argc + 4};
    printf("%d %d %d %d\n", w > 4, w > 5, w >= 5, 6 > w);
    Checked<size_t> other = 7;
    printf("%d %d\n", other > Checked<size_t>(6), Checked<size_t>(6) >= other);
    return 0;
}
