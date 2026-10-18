// The library ANGLE takes: <bitset>, <deque>, <queue>, <stack>,
// std::vector with an allocator parameter and moves, basic_string and
// basic_ostringstream over another allocator, the rest of std::string,
// the algorithms over two ranges, heaps and permutations, the atomic
// typedefs, hash<string>, std::locale for imbue.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <bitset>
#include <deque>
#include <queue>
#include <stack>
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <algorithm>
#include <functional>
#include <atomic>
#include <locale>

template <class T> struct PoolAllocator {
    typedef T value_type;
    PoolAllocator() {}
    template <class U> PoolAllocator(const PoolAllocator<U> &) {}
    template <class U> struct rebind { typedef PoolAllocator<U> other; };
    T *allocate(size_t n) { return static_cast<T *>(::operator new(n * sizeof(T))); }
    void deallocate(T *p, size_t) { ::operator delete(p); }
    bool operator==(const PoolAllocator &) const { return true; }
    bool operator!=(const PoolAllocator &) const { return false; }
};

typedef std::basic_string<char, std::char_traits<char>, PoolAllocator<char>> PString;
typedef std::basic_ostringstream<char, std::char_traits<char>, PoolAllocator<char>> PStream;

template <class T> class PVector : public std::vector<T, PoolAllocator<T>> {
  public:
    PVector() : std::vector<T, PoolAllocator<T>>() {}
    PVector(const PoolAllocator<T> &a) : std::vector<T, PoolAllocator<T>>(a) {}
    PVector(std::initializer_list<T> list) : std::vector<T, PoolAllocator<T>>(list) {}
};

static void bits() {
    std::bitset<70> b;
    b.set(3);
    b.set(69);
    b[5] = true;
    std::bitset<70> c = b << 1;
    printf("bitset %zu %d %d %d %s\n", b.count(), (int)b.test(69), (int)c.test(4), (int)(b & c).none(), std::bitset<8>(0xa5).to_string().c_str());
}

static void queues() {
    std::deque<int> d;
    for (int i = 0; i < 1000; i++) d.push_back(i);
    for (int i = 0; i < 10; i++) d.push_front(-i);
    int &held = d[500];
    for (int i = 0; i < 3000; i++) d.push_back(i);
    d.pop_front();
    printf("deque %zu %d %d %d %d\n", d.size(), d.front(), d[9], held, d.back());
    d.erase(d.begin() + 2, d.begin() + 5);
    d.insert(d.begin() + 1, 77);
    printf("deque %d %d %d\n", d[0], d[1], d[2]);
    std::queue<std::string> q;
    q.push("a");
    q.push("b");
    q.pop();
    std::stack<int> s;
    s.push(1);
    s.push(2);
    std::priority_queue<int> pq;
    for (int v : {5, 1, 9, 3}) pq.push(v);
    std::priority_queue<int, std::vector<int>, std::greater<int>> least;
    for (int v : {5, 1, 9, 3}) least.push(v);
    printf("queue %s %d %d %d\n", q.front().c_str(), s.top(), pq.top(), least.top());
    pq.pop();
    least.pop();
    printf("queue %d %d %zu\n", pq.top(), least.top(), pq.size());
}

static void vectors() {
    std::vector<std::unique_ptr<int>> owned;
    for (int i = 0; i < 9; i++) owned.emplace_back(new int(i * i));
    owned.erase(owned.begin() + 1);
    owned.insert(owned.begin(), std::make_unique<int>(-1));
    std::vector<int> v = {1, 2, 3};
    std::vector<int> w(v.rbegin(), v.rend());
    v.insert(v.begin() + 1, w.begin(), w.end());
    v.insert(v.end(), 2, 9);
    std::erase(v, 2);
    PVector<int> pv = {4, 5, 6};
    pv.push_back(7);
    printf("vector %zu %d %d %d %d %zu %d %d\n", owned.size(), *owned[0], *owned[1], *owned.back(), (int)(w < v), v.size(), v[1], pv.back());
    for (int x : v) printf("%d ", x);
    printf("\n");
}

static void strings() {
    PString s = "abc";
    s += "def";
    PString t = s.substr(1, 3);
    PStream out;
    out << t << ':' << 42;
    out.imbue(std::locale::classic());
    PString made = out.str();
    std::string plain(t.begin(), t.end());
    plain.insert(plain.begin(), 'x');
    plain.append(std::string_view("yz"));
    printf("string %s %s %s %d %d %zu\n", t.c_str(), made.c_str(), plain.c_str(), (int)plain.starts_with("xb"), (int)plain.ends_with('z'), plain.find_last_of("bc"));
    std::string r(plain.rbegin(), plain.rend());
    printf("string %s %d %zu\n", r.c_str(), plain.compare(1, 2, "bc"), std::hash<std::string>()("abc") == std::hash<std::string_view>()("abc") ? (size_t)1 : (size_t)0);
}

static void algorithms() {
    int a[] = {1, 2, 3, 4, 5};
    int b[] = {1, 2, 3, 4, 5};
    int c[] = {2, 4, 6};
    std::vector<int> out;
    std::set_intersection(a, a + 5, c, c + 3, std::back_inserter(out));
    std::set_difference(a, a + 5, c, c + 3, std::back_inserter(out));
    std::vector<int> heap = {3, 1, 4, 1, 5, 9, 2, 6};
    std::make_heap(heap.begin(), heap.end());
    std::pop_heap(heap.begin(), heap.end());
    int top = heap.back();
    std::vector<int> perm = {1, 2, 3};
    int perms = 1;
    while (std::next_permutation(perm.begin(), perm.end())) perms++;
    int *found = std::search(a, a + 5, c, c + 1);
    std::vector<int> parts = {1, 2, 3, 4, 5, 6};
    auto middle = std::partition(parts.begin(), parts.end(), [](int x) { return x % 2 == 0; });
    printf("algorithms %d %d %d %d %d %d %d\n", (int)std::equal(a, a + 5, b, b + 5), (int)std::equal(a, a + 5, c, c + 3), top, perms, (int)(found - a), (int)(middle - parts.begin()), (int)std::is_partitioned(parts.begin(), parts.end(), [](int x) { return x % 2 == 0; }));
    for (int x : out) printf("%d ", x);
    printf("\n");
}

static void atomics() {
    std::atomic_uint32_t state = 0;
    std::atomic_int64_t big(5);
    state.fetch_add(3);
    big += 10;
    printf("atomic %u %lld\n", state.load(), (long long)big.load());
}

int main() {
    bits();
    queues();
    vectors();
    strings();
    algorithms();
    atomics();
    return 0;
}
