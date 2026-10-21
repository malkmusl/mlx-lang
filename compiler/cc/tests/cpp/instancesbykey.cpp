// A template's instances are found by their key: the same instance
// named many times is one class or one function; a member defined
// after the instances were made reaches each of them; an instance
// named as a candidate and never called, and one whose address is
// taken but which is never called, get their bodies at the end.
#include <stdio.h>

template<typename T> struct Box {
    T value;
    Box(T v) : value(v) { }
    int twice() const;
    static int count;
};
template<typename T> int Box<T>::count = 0;

Box<int> a(1);
Box<long> b(2);
Box<int> c(3);
Box<char> d('x');

template<typename T> int Box<T>::twice() const { count++; return (int)value * 2; }

template<typename T> int order(T v) { return v > 0 ? 1 : 2; }
template<typename T> int order(T *v) { return v ? 3 : 4; }

template<typename T> int named(T v) { return (int)v + 100; }
int (*taken)(int) = named<int>;

template<typename T> struct Outer {
    template<typename U> struct Inner { static int sum() { return sizeof(T) + sizeof(U); } };
};

int main(int argc, char **argv)
{
    int p = 0;
    printf("%d %d %d %d\n", a.twice(), b.twice(), c.twice(), d.twice());
    printf("%d %d %d\n", Box<int>::count, Box<long>::count, Box<char>::count);
    printf("%d %d %d\n", order(argc), order(&p), order((int *)0));
    printf("%d\n", taken(argc));
    printf("%d %d %d\n", Outer<int>::Inner<char>::sum(), Outer<int>::Inner<int>::sum(), Outer<char>::Inner<char>::sum());
    Box<Box<int>> nested(a);
    int t = nested.value.twice();
    printf("%d %d\n", t, Box<int>::count);
    return argc;
}
