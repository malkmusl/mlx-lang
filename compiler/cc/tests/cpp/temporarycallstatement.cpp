// Statements that begin with a temporary of a class: a member function
// of `T()` called, a functor made with arguments and called at once, and
// one made from a braced list (no declarations of variables, as the
// conformance suite's `ShaderSourceBuilder(...)(x)` has them).
#include <stdio.h>

static int calls = 0;

struct Counter
{
    void bump(void) const
    {
        calls += 1;
    }
};

struct Adder
{
    Adder(int a, int b) : sum(a + b)
    {
    }
    void operator()(int c) const
    {
        calls += sum + c;
    }
    int sum;
};

struct Point
{
    int x;
    int y;
    void add(void) const
    {
        calls += x * y;
    }
};

int main()
{
    Counter().bump();
    Adder(1, 2)(3);
    Adder(10, 20)(-30);
    Point{4, 5}.add();
    printf("%d\n", calls);
    return 0;
}
