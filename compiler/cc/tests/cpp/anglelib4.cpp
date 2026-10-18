// The library ANGLE takes, 4: std::array ordered, iota and the other
// <numeric> algorithms, unordered containers filled from a range,
// atomic of a small class, the address of a const string's character,
// an enum as an unordered_map key (std::hash of an enum).
#include <stdio.h>
#include <array>
#include <atomic>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ThreadId { unsigned long value; };
enum class Kind { Vertex, Fragment, Compute };

static int parseMajor(const std::string &version) {
    size_t at = version.find('@');
    std::istringstream stream(&version[at + 1]);
    int major = 0;
    stream >> major;
    return major;
}

int main() {
    std::array<int, 2> a = {26, 1};
    printf("%d %d %d\n", (int)(a < (std::array<int, 2>{26, 2})), (int)(a > (std::array<int, 2>{25, 9})), (int)(a <= a));

    std::vector<unsigned> v(4);
    std::iota(v.begin(), v.end(), 10u);
    std::vector<int> sums(4);
    std::partial_sum(v.begin(), v.end(), sums.begin());
    printf("%u %u %d %d %d\n", v[0], v[3], sums[3], std::accumulate(v.begin(), v.end(), 0, [](int x, unsigned y) { return x + (int)y * 2; }), std::inner_product(v.begin(), v.end(), v.begin(), 0));

    std::unordered_map<unsigned, int *> images, invalid;
    int x = 5;
    images[1] = &x;
    images[2] = &x;
    invalid.insert(images.begin(), images.end());
    std::unordered_set<int> seen;
    std::vector<int> values = {3, 1, 3, 2};
    seen.insert(values.begin(), values.end());
    printf("%zu %d %zu\n", invalid.size(), *invalid[2], seen.size());

    std::atomic<ThreadId> owner;
    owner.store(ThreadId{5});
    ThreadId expected{4};
    bool first = owner.compare_exchange_strong(expected, ThreadId{9});
    bool second = owner.compare_exchange_strong(expected, ThreadId{9});
    printf("%lu %d %d %lu\n", expected.value, (int)first, (int)second, owner.load().value);

    std::unordered_map<Kind, const char *> names;
    names[Kind::Fragment] = "fragment";
    names[Kind::Compute] = "compute";
    printf("%d %s\n", parseMajor("OpenGL ES 3.2 build 1.13@5776728"), names[Kind::Compute]);
    return 0;
}
