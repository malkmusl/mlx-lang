// A non-type template parameter of a function type named by a typedef
// (`template<Operation operation>`): a pointer to such a function.
#include <cstdio>

struct Wrapper { int value; };

struct Operations {
    using Operation = int(int, Wrapper*);

    template<Operation operation, bool twice = false>
    static int call(int x)
    {
        Wrapper w { 10 };
        int result = operation(x, &w);
        return twice ? operation(result, &w) : result;
    }
};

int add(int x, Wrapper* w) { return x + w->value; }
int times(int x, Wrapper* w) { return x * w->value; }

int main(int argc, char** argv)
{
    std::printf("%d %d %d\n", Operations::call<add>(argc), Operations::call<times>(argc), Operations::call<add, true>(argc));
    return 0;
}
