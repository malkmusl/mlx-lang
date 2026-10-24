// std::pair made piecewise (std::piecewise_construct and two tuples of
// arguments: unordered_map::emplace in ANGLE's Vulkan pipeline cache), and
// unordered_map and unordered_set compared with ==.
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <tuple>
#include <string>
#include <cstdio>
struct Pipeline { int id; Pipeline(Pipeline &&o) : id(o.id) { o.id = -1; } explicit Pipeline(int i) : id(i) {} };
struct Helper { int a; int b; Helper(Pipeline &&p, int &&f) : a(p.id), b(f) {} };
int main() {
    std::unordered_map<std::string, Helper> map;
    Pipeline p(7);
    int feedback = 3;
    std::string key = "k";
    auto r = map.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::move(p), std::move(feedback)));
    std::unordered_map<int, int> a{{1, 2}, {3, 4}}, b{{3, 4}, {1, 2}}, c{{1, 2}};
    std::unordered_set<int> s{1, 2}, t{2, 1};
    std::pair<std::string, int> q(std::piecewise_construct, std::forward_as_tuple("xy", 1), std::forward_as_tuple(5));
    printf("%d %d %d %d %d %d %d %s %d\n", r.second, map.at("k").a, map.at("k").b, p.id, a == b, a == c, s == t, q.first.c_str(), q.second);
}
