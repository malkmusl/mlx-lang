// Special members defaulted outside the class (also of a class template)
// copy and move the members.
#include <stdio.h>

template <class T, int N>
struct Fixed {
    Fixed() {}
    Fixed(const Fixed &other);
    Fixed &operator=(const Fixed &other);
    void push(T value) { items[size++] = value; }
    T items[N];
    int size = 0;
};

template <class T, int N>
Fixed<T, N>::Fixed(const Fixed &other) = default;

template <class T, int N>
Fixed<T, N> &Fixed<T, N>::operator=(const Fixed &other) = default;

struct Plain {
    Plain() {}
    Plain(const Plain &other);
    Plain &operator=(const Plain &other);
    int a = 1;
    long b = 2;
};

Plain::Plain(const Plain &other) = default;
Plain &Plain::operator=(const Plain &other) = default;

int main() {
    Fixed<long, 4> one;
    one.push(7);
    one.push(9);
    Fixed<long, 4> two = one;
    printf("copy: %d %ld %ld\n", two.size, two.items[0], two.items[1]);
    Fixed<long, 4> three;
    three = one;
    printf("assign: %d %ld %ld\n", three.size, three.items[0], three.items[1]);
    Plain p;
    p.a = 5;
    p.b = 6;
    Plain q = p;
    Plain r;
    r = p;
    printf("plain: %d %ld %d %ld\n", q.a, q.b, r.a, r.b);
    return 0;
}
