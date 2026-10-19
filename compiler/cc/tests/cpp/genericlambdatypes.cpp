// A generic lambda's body naming a local type of the function around
// (`using ResultType = ...;`, WebCore's MetaConsumer), its call operator
// read for an instance once that function is put aside: the type is the
// closure's ("undeclared identifier 'ResultType'"), in a member function
// of a class and of a class template's instance, and a function template.
#include <stdio.h>
#include <utility>

template<typename F, typename V> auto visit(F &&f, const V &v) { return f(v); }

template<typename T> struct Box { T v; };
template<typename T> struct Consumer {
    static auto consume(int x)
    {
        using ResultType = Box<T>;
        return visit([](auto &&value) { return ResultType { value }; }, x);
    }
};
template<typename T> auto consumeFree(int x)
{
    using ResultType = Box<T>;
    return visit([](auto &&value) { return ResultType { value }; }, x);
}
struct Plain {
    static auto consume(int x)
    {
        using ResultType = Box<long>;
        typedef Box<char> Other;
        return visit([](auto &&value) { return ResultType { value + (long)sizeof(Other) }; }, x);
    }
};

int main(int argc, char **argv)
{
    printf("%d %d %ld\n", Consumer<int>::consume(9).v, consumeFree<int>(8).v, Plain::consume(7).v);
    return argc;
}
