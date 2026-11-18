// A name brought in by two using-declarations, each of a function
// template of another namespace: a call finds both templates (the
// conformance suite's `using de::min; using tcu::min;`), also when its
// arguments are the results of overloaded member functions.
#include <stdio.h>

namespace de
{
template <typename T>
inline T min(T x, T y)
{
    return x <= y ? x : y;
}
} // namespace de

namespace tcu
{
template <typename T, int Size>
struct Vector
{
    T m[Size];
    T w(void) const
    {
        return m[Size - 1];
    }
    T &w(void)
    {
        return m[Size - 1];
    }
};

template <typename T, int Size>
Vector<T, Size> min(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<T, Size> r;
    for (int i = 0; i < Size; i++)
        r.m[i] = a.m[i] < b.m[i] ? a.m[i] : b.m[i];
    return r;
}
} // namespace tcu

using de::min;
using tcu::min;

int main()
{
    const tcu::Vector<float, 2> a = {{1, 5}};
    tcu::Vector<float, 2> b       = {{3, 2}};
    tcu::Vector<float, 2> c       = min(a, b);
    float f                       = min(2.5f, 1.5f);
    printf("%g %g %g %g\n", c.m[0], c.m[1], f, min(a.w(), b.w()));
    return 0;
}
