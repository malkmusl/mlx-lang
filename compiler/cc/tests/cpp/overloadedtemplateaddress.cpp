// `f<args>` of several function templates of the name, taken as a pointer
// to a function of one type (a table's member, a variable): the instance
// of the template whose instance has that type (ANGLE's
// Initialize4ComponentData<GLbyte, 0, 0, 0, 0x7F>, declared with an
// ImageLoadContext in loadimage.h and defined without one).
#include <stdio.h>

struct Context { int z; };
template <typename T, unsigned v> void init(const Context &, int);
template <typename T, unsigned v> void init(int x) { printf("init %d %u %d\n", x, v, (int)sizeof(T)); }
template <typename T> T twice(T x) { return x * 2; }
template <typename T> T twice(T x, T y) { return (x + y) * 2; }

using Init = void (*)(int);
struct Info { int id; Init function; };

int main() {
    static constexpr Info kInfo[] = { {1, nullptr}, {2, init<signed char, 0x7F>} };
    Init direct = init<short, 5>;
    int (*one)(int) = twice<int>;
    int (*two)(int, int) = twice<int>;
    kInfo[1].function(3);
    direct(4);
    printf("%d %d %d\n", kInfo[0].id, one(5), two(1, 2));
    return 0;
}
