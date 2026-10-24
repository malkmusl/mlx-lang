// A constexpr integer initialized as `= { value }` (WebCore's
// `static constexpr unsigned maxNameLength = { 1024 };`) is the constant
// in the braces, as `{ value }` is.
#include <stdio.h>

struct Lock {
    static constexpr unsigned maxNameLength = { 1024 };
    static constexpr int none = { };
    static const int other = { 7 };
    int field = { 3 };
};

constexpr long limit = { 1L << 40 };
static const long global = { 5 };

int main(int argc, char **)
{
    constexpr int local = { 9 };
    char buffer[Lock::maxNameLength / 512];
    static_assert(limit > 1000 && local == 9);
    Lock l;
    printf("%u %d %d %d %ld %ld %d %d\n", Lock::maxNameLength, Lock::none, Lock::other, l.field, limit, global, local, (int)sizeof(buffer));
    return argc;
}
