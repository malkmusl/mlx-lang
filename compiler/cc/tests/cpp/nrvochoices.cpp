// `return local;` of a class that cannot move by its bytes is made in
// place when every return of the function returns the same local. The
// choices are kept per function: a function whose returns are read
// while another's wait (a lambda, a local class's member, a template
// instance made inside the body, a member function of a local class)
// decides for itself, and the one around is not moved out for the
// inner one's returns.
#include <stdio.h>

static int made = 0;
static int moves = 0;

struct Tracked {
    int value;
    int *self;
    Tracked(int v) : value(v), self(&value) { made++; }
    Tracked(const Tracked &other) : value(other.value), self(&value) { made++; }
    Tracked(Tracked &&other) : value(other.value), self(&value) { made++; moves++; }
    ~Tracked() { }
};

Tracked same(int n)
{
    Tracked t(n);
    if (n > 5) {
        t.value += 1;
        return t;
    }
    return t;
}

Tracked withLambda(int n)
{
    Tracked outer(n);
    auto inner = [](int m) {
        Tracked x(m);
        if (m > 5) return x;
        x.value = 0;
        return x;
    };
    outer.value += inner(n).value;
    if (n > 100) return outer;
    return outer;
}

template<typename T> T twice(T v)
{
    T r(v.value * 2);
    if (v.value > 100) return r;
    return r;
}

Tracked viaTemplate(int n)
{
    Tracked t(n);
    Tracked u(twice(t).value);
    if (n > 100) return u;
    return u;
}

Tracked localClass(int n)
{
    struct Maker {
        Tracked make(int m) {
            Tracked p(m);
            if (m > 5) return p;
            p.value = 0;
            return p;
        }
    };
    Tracked t(n);
    t.value += Maker { }.make(n).value;
    if (n > 100) return t;
    return t;
}

// Several returns of different locals, and a return of a temporary
// beside one of a local: the values alone.
Tracked different(int n)
{
    Tracked a(n);
    Tracked b(n + 1);
    if (n > 5) return a;
    return b;
}

Tracked mixed(int n)
{
    Tracked a(n);
    if (n > 5) return Tracked(99);
    return a;
}

int main(int argc, char **argv)
{
    Tracked s = same(argc + 6);
    printf("same %d made %d moves %d\n", s.value, made, moves);
    made = moves = 0;
    Tracked l = withLambda(argc + 6);
    printf("lambda %d made %d moves %d\n", l.value, made, moves);
    made = moves = 0;
    Tracked v = viaTemplate(argc + 6);
    printf("template %d made %d moves %d\n", v.value, made, moves);
    made = moves = 0;
    Tracked c = localClass(argc + 6);
    printf("local class %d made %d moves %d\n", c.value, made, moves);
    printf("%d %d %d %d\n", different(argc + 6).value, different(argc).value, mixed(argc + 6).value, mixed(argc).value);
    return argc;
}
