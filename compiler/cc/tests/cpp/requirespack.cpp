// A trailing requires-clause naming a function parameter pack
// (`requires (requires { L(args...); })`, WebCore's
// PrimitiveNumericWrapperBase constructor), on a constructor template of
// a partial specialization and through a class inheriting it.
#include <stdio.h>
#include <utility>

struct Length {
    int value;
    Length(int x) : value(x) { }
    Length(int x, int y) : value(x * 10 + y) { }
};

template<class T> struct Wrapper;
template<class T> struct Tag { };
template<class T> struct Wrapper<Tag<T>> {
    int value;
    template<typename... Args>
    Wrapper(Args&&... args) requires (requires { { Length(args...) }; })
        : value(Length(std::forward<Args>(args)...).value)
    {
    }
    Wrapper(const char *text) : value(text[0]) { }
};

struct Position : Wrapper<Tag<float>> {
    using Wrapper<Tag<float>>::Wrapper;
};

int main(int argc, char **argv)
{
    Wrapper<Tag<int>> one(3);
    Wrapper<Tag<int>> two(4, 5);
    Wrapper<Tag<int>> text("A");
    Position position(6, 7);
    printf("%d %d %d %d\n", one.value, two.value, text.value, position.value);
    return argc - 2;
}
