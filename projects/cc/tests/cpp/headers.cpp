#include <cstdio>
#include <concepts>
#include <bit>
#include <array>
#include <span>
#include <tuple>
#include <optional>
#include <iterator>
#include <functional>
#include <atomic>
#include <mutex>
#include <expected>
#include <memory>
#include <utility>

template <std::integral T> T twice(T v) { return v * 2; }

struct Big { int a; int b; Big(int x, int y) : a(x), b(y) {} };

int sumSpan(std::span<const int> s) { int t = 0; for (int v : s) t += v; return t; }

std::optional<int> half(int v) { if (v % 2) return std::nullopt; return v / 2; }

std::expected<int, const char*> parse(int v) {
    if (v < 0) return std::unexpected<const char *>("negative");
    return v + 1;
}

int main() {
    printf("twice %d\n", twice(21));
    printf("popcount %d bit_width %d has_single_bit %d\n", std::popcount(0xF0u), (int)std::bit_width(1000u), (int)std::has_single_bit(64u));
    std::array<int, 4> arr = {1, 2, 3, 4};
    printf("array %d size %d back %d\n", arr[2], (int)arr.size(), arr.back());
    printf("span %d\n", sumSpan(arr));
    std::tuple<int, char, double> tup(7, 'x', 2.5);
    printf("tuple %d %c %g size %d\n", std::get<0>(tup), std::get<1>(tup), std::get<2>(tup), (int)std::tuple_size<decltype(tup)>::value);
    auto t2 = std::make_tuple(1, 2);
    printf("made %d\n", std::get<1>(t2));
    std::optional<int> o = half(10);
    std::optional<int> n = half(3);
    printf("optional %d %d %d %d\n", (int)o.has_value(), *o, (int)n.has_value(), n.value_or(-1));
    std::optional<Big> ob;
    ob.emplace(3, 4);
    printf("emplace %d %d\n", ob->a, ob->b);
    std::function<int(int)> fn = [](int v) { return v + 100; };
    printf("function %d\n", fn(5));
    std::atomic<int> counter(0);
    counter.fetch_add(5);
    counter++;
    printf("atomic %d\n", counter.load());
    std::mutex m;
    { std::lock_guard<std::mutex> g(m); printf("locked\n"); }
    auto e1 = parse(4);
    auto e2 = parse(-1);
    printf("expected %d %d %s\n", (int)e1.has_value(), *e1, e2.error());
    auto up = std::make_unique<Big>(8, 9);
    std::unique_ptr<Big> up2 = std::move(up);
    printf("unique %d %d\n", (int)(up == nullptr), up2->b);
    auto sp = std::make_shared<Big>(1, 2);
    std::shared_ptr<Big> sp2 = sp;
    printf("shared %d use %d\n", sp2->a, (int)sp.use_count());
    int x = 3;
    printf("addressof %d\n", (int)(std::addressof(x) == &x));
    int raw[3] = {5, 6, 7};
    printf("rbegin %d data %d\n", *std::rbegin(raw), *std::data(arr));
    std::pair<int, int> p = std::make_pair(1, 2);
    printf("pair %d %d\n", p.first, p.second);
    printf("hash %d\n", (int)(std::hash<int>()(42) == 42));
    printf("plus %d\n", std::plus<int>()(2, 3));
    return 0;
}
