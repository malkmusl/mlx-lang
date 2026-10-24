// An instance being read is told apart from an instance of another
// template of the same name and arguments (WebCore's Style::Length<R, V>
// completing CSS::Length<R, V> while it is read).
#include <stdio.h>

namespace css {
template<class T> struct Length {
    using Raw = T;
    static int base() { return 10; }
};
}

namespace style {
template<class T> struct Length {
    // (A typedef names the instance only; the member asked for next
    // needs it whole.)
    using CSS = css::Length<T>;
    using Raw = typename CSS::Raw;
    static int value() { return (int)sizeof(Raw) + CSS::base(); }
};
}

int main(int argc, char **argv) {
    printf("%d %d\n", style::Length<int>::value(), style::Length<double>::value());
    return argc - 2;
}
