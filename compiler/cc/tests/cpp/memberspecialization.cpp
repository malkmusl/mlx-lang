// Member templates of a class template's instance specialized explicitly
// (`template <> template <> ...`), as WebCore's ColorMatrix.h has them: a
// conversion function template whose result is a template-id
// (`operator M<C2, R2>()`, its parameters deduced from the target) and a
// member function template given its arguments (`M<3, 3>::g<7>`).
#include <cstdio>
#include <cstddef>

template<size_t C, size_t R>
class M {
public:
    constexpr M(int v) : value(v) { }
    template<size_t C2, size_t R2> constexpr operator M<C2, R2>() const;
    template<size_t N> int g() const { return value * 100 + static_cast<int>(N); }
    int value;
};

template<> template<> constexpr M<3, 3>::operator M<5, 4>() const
{
    return M<5, 4> { value * 10 };
}

template<> template<> int M<3, 3>::g<7>() const { return value + 7; }

static int take(M<5, 4> m) { return m.value; }

int main()
{
    M<3, 3> a { 4 };
    M<5, 4> b = a;
    M<5, 4> c = M<5, 4>(a);
    std::printf("%d %d %d\n", b.value, c.value, take(a));
    std::printf("%d %d\n", a.g<7>(), a.g<2>());
    return b.value;
}
