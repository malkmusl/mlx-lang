// Member function templates with ref-qualifiers: `&&` for an rvalue
// object, `&` and `const &` for an lvalue (mpark::variant's
// `get_alt<I>() &&`: a moved variant's alternative is an rvalue, which a
// visitor taking `String &&` takes, and only that).
#include <stdio.h>
#include <type_traits>
#include <utility>

struct Storage {
    int value;
    template<int I> int &get() & { return value; }
    template<int I> const int &get() const & { return value; }
    template<int I> int &&get() && { return std::move(value); }
    friend Storage &asBase(Storage &s) { return s; }
    friend Storage &&asBase(Storage &&s) { return std::move(s); }
};

template<class V> decltype(auto) alternative(V &&v) { return asBase(std::forward<V>(v)).template get<0>(); }

int category(int &) { return 1; }
int category(const int &) { return 2; }
int category(int &&) { return 3; }

int main(int argc, char **argv)
{
    Storage s { argc };
    const Storage &c = s;
    printf("%d %d %d\n", (int)std::is_same_v<decltype(std::move(s).get<0>()), int &&>, (int)std::is_same_v<decltype(s.get<0>()), int &>, (int)std::is_same_v<decltype(c.get<0>()), const int &>);
    printf("%d %d\n", (int)std::is_same_v<decltype(alternative(std::move(s))), int &&>, (int)std::is_same_v<decltype(alternative(s)), int &>);
    printf("%d %d %d %d\n", category(s.get<0>()), category(c.get<0>()), category(std::move(s).get<0>()), category(alternative(Storage { 4 })));
    return alternative(std::move(s)) - 2;
}
