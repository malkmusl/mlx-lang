// An rvalue stream written to is itself, its class kept: a free
// `operator<<(Stream &&, const T &)` binds the temporary better than the
// base's member operator (the object converted to the base), also along
// a chain of them.
#include <sstream>
#include <string>
#include <stdio.h>
#include <type_traits>

struct Base {
    Base &operator<<(char c) { printf("member %c\n", c); return *this; }
};
struct Derived : Base {
    void done() { printf("derived\n"); }
};
template <class S, class T, class = typename std::enable_if<!std::is_lvalue_reference<S>::value && std::is_base_of<Base, S>::value>::type>
S &&operator<<(S &&out, const T &value)
{
    printf("rvalue ");
    out << value;
    return static_cast<S &&>(out);
}

int main() {
    (Derived() << 'a' << 'b').done();
    std::string name = "uniform";
    for (int index = 1; index < 3; ++index) {
        auto element = (std::ostringstream() << name << '[' << index << ']').str();
        printf("%s\n", element.c_str());
    }
    std::ostringstream lvalue;
    lvalue << "plain " << 42;
    printf("%s\n", lvalue.str().c_str());
    return 0;
}
