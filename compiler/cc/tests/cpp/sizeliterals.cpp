// C++23's size_t literals: `1uz` (and `zu`, `UZ`) a size_t, `1z` its
// signed type (WebCore's BlobResourceHandleBase.cpp: `rangeSize +=
// 1uz;`, "invalid numeric constant").
#include <stdio.h>
#include <stddef.h>
#include <type_traits>

int main(int argc, char **argv)
{
    auto a = 1uz;
    auto b = 2ZU;
    auto c = 3z;
    auto d = 0x10uz;
    printf("%d %d %d %d\n", (int)std::is_same_v<decltype(a), size_t>, (int)std::is_same_v<decltype(b), size_t>, (int)std::is_same_v<decltype(c), std::make_signed_t<size_t>>, (int)std::is_same_v<decltype(d), size_t>);
    printf("%zu %zu %ld %zu %zu\n", a, b, (long)c, d, sizeof(1z));
    return argc;
}
