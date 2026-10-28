// Unary -, ~ and + of a class with no such operator but a conversion to
// a scalar (WTF's Checked: `TrustedImm32(-baseOffset)` of YarrJIT, `+x`
// given to printf), and + of a lambda (its function pointer).
#include <stdio.h>
struct Record { bool overflowed() const { return false; } };
template<typename T, typename H = Record> class Checked : public H {
public:
    constexpr Checked() : m_value(0) { }
    constexpr Checked(const Checked& value) : m_value(value.m_value) { }
    template<typename U> constexpr Checked(U value) : m_value(value) { }
    Checked& operator=(Checked rhs) { m_value = rhs.m_value; return *this; }
    bool operator!() const { return !m_value; }
    explicit operator bool() const { return m_value; }
    operator T() const { return m_value; }
    T m_value;
};
template<typename U, typename V, typename H> static inline Checked<U, H> operator-(Checked<U, H> lhs, V rhs) { return Checked<U, H>(lhs.m_value - rhs); }
template<typename U, typename V, typename H> static inline Checked<U, H> operator-(U lhs, Checked<V, H> rhs) { return Checked<U, H>(lhs - rhs.m_value); }
template<typename U, typename V, typename H> static inline Checked<U, H> operator+(Checked<U, H> lhs, V rhs) { return Checked<U, H>(lhs.m_value + rhs); }
struct Imm { explicit Imm(int v) : m_value(v) { } int m_value; };
int take(Imm i) { return i.m_value; }
int apply(int (*f)(int), int v) { return f(v); }
int main()
{
    auto offset = Checked<int, Record>(-static_cast<int>(5u));
    auto total2 = 3 + offset;
    int total = 0;
    if (offset < 0)
        total = take(Imm(-offset));
    int plain = -offset;
    printf("%d %d %d %d %d %d\n", total, plain, +offset, (int)total2, ~offset, apply(+[](int x) { return x - 1; }, 4));
    return 0;
}
