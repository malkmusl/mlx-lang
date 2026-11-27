// ++ and -- in front of a container's begin() and end(): an iterator by
// value, which is a pointer in mlxlibcpp (a class in libstdc++), stepped
// as a temporary and read; the container is left as it was (the
// conformance suite's `*--vector.end()`).
#include <stdio.h>
#include <string>
#include <vector>

int main()
{
    std::vector<int> pool(3);
    pool[0] = 1;
    pool[1] = 4;
    pool[2] = 9;
    int last   = *--pool.end();
    int second = *++pool.begin();
    std::string word = "abc";
    char final = *--word.end();
    printf("%d %d %c %zu %d\n", last, second, final, pool.size(), pool[0]);
    return 0;
}
