// An object of a class copied by its bytes, assigned a value of another
// type: a temporary made by the class's converting constructor is what
// is assigned, not the value's bytes (ANGLE's CheckedNumeric: `n = m`).
#include <stdio.h>

template <typename T> struct Strict {
    T v;
    constexpr Strict(T x) : v(x) {}
};

template <typename T> class Checked {
  public:
    constexpr Checked() = default;
    template <typename Src> constexpr Checked(const Checked<Src> &rhs) : value_(rhs.value_), valid_(rhs.valid_) {}
    template <typename Src> constexpr Checked(Src value) : value_(static_cast<T>(value)), valid_(true) {}
    template <typename Src> constexpr Checked(Strict<Src> value) : value_(value.v), valid_(true) {}
    T value_ = 0;
    bool valid_ = false;
};

struct Plain {
    Plain() = default;
    Plain(int n) : twice(n * 2) {}
    int twice = 0;
};

int main() {
    Checked<unsigned> n;
    unsigned m = 9;
    printf("%u %d\n", n.value_, (int)n.valid_);
    n = m;
    printf("%u %d\n", n.value_, (int)n.valid_);
    n = 70000L;
    printf("%u %d\n", n.value_, (int)n.valid_);
    n = Strict<unsigned>(5);
    printf("%u %d\n", n.value_, (int)n.valid_);
    Checked<unsigned short> narrow;
    narrow = n;
    printf("%u %d\n", (unsigned)narrow.value_, (int)narrow.valid_);
    Plain p;
    p = 21;
    printf("%d\n", p.twice);
    return 0;
}
