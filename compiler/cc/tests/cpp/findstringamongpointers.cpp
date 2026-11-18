// std::find of a std::string among const char * values: each compared
// with operator== of a const char * and a string.
#include <stdio.h>
#include <algorithm>
#include <string>
#include <vector>

template <typename I, typename K>
inline bool contains(const I &begin, const I &end, const K &item)
{
    const I it = std::find(begin, end, item);
    return (it != end);
}

int main()
{
    std::vector<const char *> names;
    names.push_back("VK_KHR_a");
    names.push_back("VK_EXT_descriptor_indexing");
    const std::string wanted = "VK_EXT_descriptor_indexing";
    const std::string other  = "VK_KHR_b";
    printf("%d %d\n", contains(names.begin(), names.end(), wanted), contains(names.begin(), names.end(), other));
    return 0;
}
