// A constrained template parameter's concept, unqualified, is the one the
// template head sees, though a member of the class declared after the
// member template has its name (JavaScriptCore's FTLLowerDFGToB3.cpp:
// `template<typename Generator, isFunction Func1, isFunction Func2>`
// beside the member function `isFunction(LValue, SpeculatedType)`), in
// a named namespace, an unnamed one in it and the global one.
#include <stdio.h>
#include <type_traits>

template<typename T>
concept isIntegral = std::is_integral_v<T>;

namespace outer {
namespace {

template<typename T>
concept isFunction = std::is_function_v<std::remove_pointer_t<T>>;

int twice(int x) { return x * 2; }
long plusOne(int x) { return x + 1; }

class Lowering {
public:
    template<typename Tag, isFunction Func1, isFunction Func2>
    long both(int v, Func1 first, Func2 second) { return first(v) + second(v) + Tag::value; }

    template<isIntegral I>
    I doubled(I v) { return v * 2; }

    long run()
    {
        auto first = twice;
        auto second = plusOne;
        return both<Tag>(20, first, second) + doubled(3) + (isFunction(4) ? 100 : 0) + (isIntegral() ? 1000 : 0);
    }

    struct Tag { static constexpr int value = 5; };
    bool isFunction(int x) const { return x > 3; }
    bool isIntegral() const { return true; }
};

}
}

int main()
{
    outer::Lowering lowering;
    printf("%ld\n", lowering.run());
    return 0;
}
