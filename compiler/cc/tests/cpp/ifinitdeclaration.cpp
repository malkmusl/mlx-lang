// An if with an init statement whose condition declares a variable
// (`if (auto f = ...; RefPtr rotate = dynamicDowncast<...>(f))` in
// WebCore's StyleRotate.cpp: "a class template is not a value"), its
// class deduced; both destroyed after the else, the condition's first.
#include <stdio.h>

struct Loud {
    int v;
    Loud(int x) : v(x) { printf("make %d\n", v); }
    ~Loud() { printf("destroy %d\n", v); }
};
template<class T> struct Ptr {
    T *p;
    Ptr(T *q) : p(q) { printf("ptr\n"); }
    ~Ptr() { printf("~ptr\n"); }
    explicit operator bool() const { return p; }
};
int *pick(int *q) { return q; }

int g(int *q)
{
    if (Loud x { 1 }; Ptr r = pick(q))
        return *r.p + x.v;
    else if (int n = 7; n > 3)
        return n;
    return -1;
}

int main(int argc, char **argv)
{
    int v = 5;
    printf("%d\n", g(&v));
    printf("%d\n", g(nullptr));
    return argc;
}
