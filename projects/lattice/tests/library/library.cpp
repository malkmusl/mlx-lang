// The library test: the C++ library's classes as records like any other,
// in a program of units (one here). std::map and std::set on their tree,
// std::shared_ptr and its control block, std::mutex under a lock_guard,
// std::optional, std::vector from a braced list, std::sort with and
// without a comparison, lambdas with and without captures, std::function.
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <optional>
#include <functional>
#include <vector>
#include <algorithm>

namespace library {

struct Item {
    int id;
    int weight;
};

struct Registry {
    std::mutex lock;
    std::map<int, Item> items;
    std::set<int> seen;
    std::shared_ptr<Item> last;
    std::optional<int> best;

    void add(int id, int weight) {
        std::lock_guard<std::mutex> guard(lock);
        items[id] = Item{id, weight};
        seen.insert(weight);
        last = std::make_shared<Item>(Item{id, weight});
        if (!best || weight > *best) {
            best = weight;
        }
    }

    int total() {
        int sum = 0;
        for (const auto &entry : items) {
            sum += entry.first * entry.second.weight;
        }
        return sum;
    }
};

template <class F> static int apply(F &&f, int v) { return f(v); }

int run() {
    Registry r;
    r.add(3, 10);
    r.add(1, 25);
    r.add(7, 4);
    r.add(3, 11);
    int total = r.total();
    total += static_cast<int>(r.seen.size()) * 1000;
    total += r.last->weight * 7;
    total += r.best.value_or(0) * 3;
    int base = 5;
    auto scale = [base](int v) { return v * base + 1; };
    total += apply(scale, 9);
    std::vector<int> values = {5, 3, 9, 1};
    int raw[4] = {5, 3, 9, 1};
    std::sort(raw, raw + 4);
    total += raw[0] * 3 + raw[3] * 5;
    std::sort(values.begin(), values.end(), [](int a, int b) { return a > b; });
    total += values[0] * 100 + values[3];
    int count = 0;
    std::for_each(values.begin(), values.end(), [&count](int v) { if (v > 2) count++; });
    total += count * 10000;
    std::function<int(int)> twice = [](int v) { return v * 2; };
    total += twice(21);
    return total;
}

}
