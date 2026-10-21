// std::reverse_iterator converts from one over a converting iterator
// (`T *` to `const T *`: WTF::FixedVector's `m_storage ? m_storage->rend()
// : const_reverse_iterator(nullptr)`), not the other way; and a pointer to
// const does not convert to a pointer to non-const.
#include <stdio.h>
#include <iterator>
#include <type_traits>

int main(int argc, char **argv)
{
    int values[3] = { 1, 2, argc };
    std::reverse_iterator<int *> last(values + 3);
    std::reverse_iterator<const int *> constLast = last;
    bool some = values[0] == 1;
    auto either = some ? last : std::reverse_iterator<const int *>(nullptr);
    printf("%d %d %d %d %d\n", *constLast, *either,
        (int)std::is_convertible_v<std::reverse_iterator<const int *>, std::reverse_iterator<int *>>,
        (int)std::is_convertible_v<const int *, int *>, (int)std::is_convertible_v<int *, const int *>);
    return argc - 2;
}
