// The definitions and the explicit instantiation definitions: each
// named function made here (weak, as g++ makes them), and every member
// of a class instance named (its static data members, its member
// classes' members; not its member templates).
#include "instances.h"

template<typename V> int S::visit(V& v)
{
    total += v.n;
    return total;
}
template<int N> int C<N>::f() { return N; }
template<int N> int C<N>::s = N + 100;
template<int N> int C<N>::Inner::h() { return N + 1000; }
template<class T> T twice(T t) { return t + t; }
template<class T> T ns::Box<T>::get() const { return value; }
template<class T> void ns::Box<T>::set(T v) { value = v; }

template int S::visit(V1&);
template int S::visit<V2>(V2&);
template struct C<3>;
template int twice(int);
template double twice<double>(double);
template struct ns::Box<long>;
