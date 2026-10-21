// A member initializer's braces for an aggregate member, elided braces
// among them: `m_matrix { { { 1, 0, 0, 0 }, ... } }` of a
// std::array<std::array<double, 4>, 4> (WebCore's TransformationMatrix).
#include <cstdio>
#include <array>

class M {
public:
    typedef std::array<std::array<double, 4>, 4> Matrix4 __attribute__((aligned (16)));
    constexpr M()
        : m_matrix { {
            { 1, 0, 0, 0 },
            { 0, 1, 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 1 },
        } }
    {
    }
    constexpr M(double a, double b)
        : m_matrix { {
            { a, b, 0, 0 },
            { b, a, 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 1 },
        } }
    {
    }
    Matrix4 m_matrix;
};

int main()
{
    M m;
    M n(2, 3);
    std::array<std::array<int, 2>, 2> local { { { 1, 2 }, { 3, 4 } } };
    std::array<std::array<int, 2>, 2> flat = { 5, 6, 7, 8 };
    std::printf("%g %g %g %g %d %d\n", m.m_matrix[0][0], m.m_matrix[1][2], n.m_matrix[0][1], n.m_matrix[1][0], local[1][0], flat[1][1]);
    return 0;
}
