// std::distance, std::advance and std::equal_range over reverse
// iterators of a std::map (bidirectional) step them one by one: the
// reverse_iterator's `-` and `+=` are a random-access iterator's only.
// (WebCore's SampleMap takes equal_range over a map's reverse range.)
// A vector's reverse iterators still jump.
#include <algorithm>
#include <iterator>
#include <map>
#include <stdio.h>
#include <vector>

int main(int argc, char **)
{
    std::map<int, const char *> m { { 1, "a" }, { 3, "c" }, { 5, "e" }, { 7, "g" } };
    auto first = m.rbegin();
    auto last = m.rend();
    long n = std::distance(first, last);
    auto third = std::next(first, 2);
    auto range = std::equal_range(first, last, std::pair<const int, const char *>(3, ""), [](const auto &a, const auto &b) { return a.first > b.first; });
    printf("%ld %d %s %ld %ld\n", n, third->first, range.first->second, std::distance(first, range.first), std::distance(range.first, range.second));

    std::vector<int> v { 10, 20, 30, 40, 50 };
    auto r = v.rbegin();
    r += 2;
    auto r2 = v.rbegin() + 1;
    printf("%d %d %d %ld %ld\n", *r, *r2, v.rbegin()[3], (long)(v.rend() - v.rbegin()), (long)std::distance(r2, v.rend()));
    auto lower = std::lower_bound(v.rbegin(), v.rend(), 25, [](int a, int b) { return a > b; });
    printf("%d %ld\n", *lower, (long)(lower - v.rbegin()));
    return argc;
}
