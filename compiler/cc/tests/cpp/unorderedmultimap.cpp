// std::unordered_multimap: several values under one key, counted, found
// together by equal_range and erased with their key.
#include <stdio.h>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

int main()
{
    std::unordered_multimap<std::string, int> m;
    m.insert(std::make_pair(std::string("a"), 1));
    m.insert(std::make_pair(std::string("b"), 2));
    m.insert(std::make_pair(std::string("a"), 3));
    m.insert(std::make_pair(std::string("a"), 5));
    printf("%zu %zu %zu %zu\n", m.size(), m.count("a"), m.count("b"), m.count("c"));
    std::vector<int> values;
    auto range = m.equal_range("a");
    for (auto i = range.first; i != range.second; ++i)
        values.push_back(i->second);
    std::sort(values.begin(), values.end());
    for (size_t i = 0; i < values.size(); i++)
        printf("%d ", values[i]);
    printf("\n");
    size_t erased = m.erase("a");
    printf("%zu %zu %d\n", erased, m.size(), m.find("b")->second);
    return 0;
}
