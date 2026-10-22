// A destructor called explicitly that does nothing (a scalar's, through a
// template's `p->~T()`; a class's trivial one) still evaluates its
// object: `(--iter)->~T()` of Skia's AutoSTArray::reset steps back.
#include <cstdio>
template<typename T> int destroyAll(T* start, T* end) {
    int n = 0;
    T* iter = end;
    while (iter > start && n < 100) { (--iter)->~T(); n++; }
    return n;
}
template<typename T> int destroyOne(T* p) { int n = 0; (n++, p)->~T(); return n; }
struct S { ~S() {} };
int main()
{
    const int* ptrs[4] = {};
    int ints[3] = {};
    S ss[2];
    std::printf("pointers %d ints %d classes %d\n", destroyAll(ptrs, ptrs + 4), destroyAll(ints, ints + 3), destroyAll(ss, ss + 2));
    std::printf("one %d\n", destroyOne(ints));
    return 0;
}
