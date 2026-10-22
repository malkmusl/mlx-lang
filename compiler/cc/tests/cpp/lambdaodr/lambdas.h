// Lambdas of inline functions, templates, default member initializers
// and inline variables, which two files read in different orders (the
// one reads other inline bodies first): their closure classes are the
// same classes in both (Itanium C++ ABI: numbered in their context), so
// a static local of a call operator is one object, and a template's
// instance for a closure class is one instance.
#include <functional>
#include <stdio.h>

inline int tripled(int x)
{
    auto l = [](int v) { return v * 3; };
    auto m = [] { return 1; };
    return l(x) + m();
}

inline int* counter()
{
    auto l = [] {
        static int n = 0;
        return &n;
    };
    return l();
}

// (Called through its conversion to a function pointer too: the same
// static.)
inline int* counterThroughPointer()
{
    int* (*f)() = [] {
        static int n = 0;
        return &n;
    };
    return f();
}

template<typename T> int* typedCounter()
{
    auto l = [](T) {
        static int n = 0;
        return &n;
    };
    return l(T());
}

inline int* nestedCounter()
{
    auto outer = [] {
        auto inner = [] {
            static int s = 5;
            return &s;
        };
        return inner();
    };
    return outer();
}

inline int* genericCounter()
{
    auto g = [](auto x) {
        static int n = 0;
        n += (int)x;
        return &n;
    };
    return g(1);
}

inline int* mutableCounter()
{
    int k = 0;
    auto l = [k]() mutable {
        static int n = 0;
        ++k;
        return &n;
    };
    return l();
}

struct Widget {
    Widget() { }
    explicit Widget(int b) : base(b) { }
    std::function<int*()> make = [] {
        static int n = 0;
        return &n;
    };
    int base { 1 };
    std::function<int()> get = [this] { return base; };
};

// (A member of another class of the same name: another closure.)
struct Gadget {
    std::function<int*()> make = [] {
        static int n = 0;
        return &n;
    };
};

inline auto sharedLambda = [] {
    static int n = 0;
    return &n;
};

template<typename T> int* perType()
{
    static int n = 0;
    return &n;
}

inline int* closureType()
{
    auto l = [](char) { return 0; };
    return perType<decltype(l)>();
}

struct Pointers {
    int* counter;
    int* throughPointer;
    int* typed;
    int* nested;
    int* generic;
    int* mutableOne;
    int* member;
    int* memberOther;
    int* variable;
    int* type;
};

Pointers fromA();
Pointers fromB();
