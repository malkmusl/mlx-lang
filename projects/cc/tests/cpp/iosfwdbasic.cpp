// <iosfwd> declares std::basic_ostream and std::basic_istream (with
// their traits defaulted), for headers naming them in templates.
#include <iosfwd>
#include <cstdio>

template <typename T> struct Box {
    T *p;
    T *get() const { return p; }
};

// Only declared, as Skia's SkRefCnt.h does: never chosen below.
template <typename C, typename CT, typename T>
auto operator<<(std::basic_ostream<C, CT> &os, const Box<T> &box) -> decltype(os << box.get()) {
    return os << box.get();
}

template <typename C, typename CT>
int readInto(std::basic_istream<C, CT> *in) { return in == nullptr ? 1 : 2; }

template <typename S> struct IsCharStream { static const int value = 0; };
template <typename C> struct IsCharStream<std::basic_ostream<C> > { static const int value = 1; };

int main(int argc, char **argv) {
    int x = 5;
    Box<int> b = { &x };
    std::basic_istream<char> *none = nullptr;
    printf("%d %d %d\n", *b.get(), readInto(none), IsCharStream<std::basic_ostream<char, std::char_traits<char> > >::value);
    return argc;
}
