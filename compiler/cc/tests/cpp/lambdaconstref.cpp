// A captureless lambda bound to a `const F &` of a function pointer type
// F through its conversion to the pointer: as an argument of a function,
// of overloads, of a constructor, and in std::pair's braced construction
// with a `{ }` first (WTF's SocketConnection::MessageHandlers, as the
// remote inspector of JavaScriptCore for WPE writes them).
#include <stdio.h>
#include <utility>

using Callback = void (*)(int&, void*);

static int takeRef(const Callback& c) { int v = 0; c(v, nullptr); return v; }
static int takeRef2(int, const Callback& c) { int v = 0; c(v, nullptr); return v; }
static int takeRef2(double, double) { return -1; }

struct Holder {
    Callback callback;
    Holder(int, const Callback& c) : callback(c) { }
};

struct Text {
    const char* data = "";
    Text() = default;
    Text(const char* d) : data(d) { }
};

int main(int argc, char** argv)
{
    printf("ref %d\n", takeRef([](int& x, void*) { x = 6; }));
    printf("overloads %d\n", takeRef2(1, [](int& x, void*) { x = 8; }));
    Holder h(0, [](int& x, void*) { x = 7; });
    Holder braced { 0, [](int& x, void*) { x = 9; } };
    int v = 0;
    h.callback(v, nullptr);
    printf("holder %d", v);
    braced.callback(v, nullptr);
    printf(" %d\n", v);
    std::pair<Text, Callback> a { { }, [](int& x, void* data) { x += *static_cast<int*>(data); } };
    std::pair<Text, Callback> b { "named", [](int& x, void*) { x *= 2; } };
    int total = 1;
    int add = 20;
    a.second(total, &add);
    b.second(total, nullptr);
    printf("pairs %d '%s' '%s'\n", total, a.first.data, b.first.data);
    return argc - 1;
}
