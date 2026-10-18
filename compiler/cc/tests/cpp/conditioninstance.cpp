// A class template's instance named only through a reference parameter
// (`const std::function<...> &`) tested as a condition: completed for
// its explicit operator bool (Skia's AtlasPathRenderer callback).
#include <functional>
#include <cstdio>

struct Proxy {};
using Callback = std::function<bool(const Proxy *)>;

template <typename T>
struct Holder {
    explicit operator bool() const { return value != 0; }
    T value;
};

static int check(const Proxy *current, const Callback &callback, const Holder<int> &holder) {
    int r = 0;
    if (callback) r += 1;
    if (current && callback && callback(current)) r += 2;
    if (holder) r += 4;
    return r;
}

int main(int argc, char **argv) {
    Proxy p;
    Holder<int> h{3};
    printf("%d %d\n", check(&p, nullptr, h), check(&p, [](const Proxy *) { return true; }, h));
    return argc;
}
